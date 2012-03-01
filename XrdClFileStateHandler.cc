//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFileStateHandler.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDTransport.hh"

namespace
{
  //----------------------------------------------------------------------------
  // Object that does things to the FileStateHandler when kXR_open returns
  // and then calls the user handler
  //----------------------------------------------------------------------------
  class OpenHandler: public XrdClient::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      OpenHandler( XrdClient::FileStateHandler *stateHandler,
                   XrdClient::ResponseHandler  *userHandler ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdClient::XRootDStatus *status,
                                   XrdClient::AnyObject    *response,
                                   URLList                 *urlList )
      {
        using namespace XrdClient;

        //----------------------------------------------------------------------
        // Extract the statistics info
        //----------------------------------------------------------------------
        OpenInfo *openInfo = 0;
        if( status->IsOK() )
        {
          if( !response )
          {
            XRootDStatus st( stError, errInternal );
            pStateHandler->SetOpenStatus( &st, 0, urlList );
          }
          response->Get( openInfo );
        }

        //----------------------------------------------------------------------
        // Notify the state handler and the client and say bye bye
        //----------------------------------------------------------------------
        pStateHandler->SetOpenStatus( status, openInfo, urlList );
        delete response;
        pUserHandler->HandleResponse( status, 0, urlList );
        delete this;
      }

    private:
      XrdClient::FileStateHandler *pStateHandler;
      XrdClient::ResponseHandler  *pUserHandler;
  };

  //----------------------------------------------------------------------------
  // Object that does things to the FileStateHandler when kXR_close returns
  // and then calls the user handler
  //----------------------------------------------------------------------------
  class CloseHandler: public XrdClient::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      CloseHandler( XrdClient::FileStateHandler *stateHandler,
                    XrdClient::ResponseHandler  *userHandler ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdClient::XRootDStatus *status,
                                   XrdClient::AnyObject    *response,
                                   URLList                 *urlList )
      {
        pStateHandler->SetCloseStatus( status );
        pUserHandler->HandleResponse( status, response, urlList );
        delete this;
      }

    private:
      XrdClient::FileStateHandler *pStateHandler;
      XrdClient::ResponseHandler  *pUserHandler;
  };

  //----------------------------------------------------------------------------
  // Stateful message handler
  //----------------------------------------------------------------------------
  class StatefulHandler: public XrdClient::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StatefulHandler( XrdClient::FileStateHandler *stateHandler,
                       XrdClient::ResponseHandler  *userHandler,
                       XrdClient::Message          *message ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler ),
        pMessage( message )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdClient::XRootDStatus *status,
                                   XrdClient::AnyObject    *response,
                                   URLList                 *urlList )
      {
        using namespace XrdClient;
        std::auto_ptr<StatefulHandler> self( this );
        std::auto_ptr<XRootDStatus>    statusPtr( status );
        std::auto_ptr<AnyObject>       responsePtr( response );
        std::auto_ptr<URLList>         urlListPtr( urlList );

        //----------------------------------------------------------------------
        // Houston we have a problem...
        //----------------------------------------------------------------------
        if( !status->IsOK() )
        {
          pStateHandler->HandleStateError( status, pMessage, pUserHandler );
//          return;
        }

        //----------------------------------------------------------------------
        // We have been sent out elsewhere
        //----------------------------------------------------------------------
        if( status->IsOK() && status->code == suXRDRedirect )
        {
          URL *target = 0;
          response->Get( target );
          pStateHandler->HandleRedirection( target, pMessage, pUserHandler );
          return;
        }

        //----------------------------------------------------------------------
        // We're clear
        //----------------------------------------------------------------------
        statusPtr.release();
        responsePtr.release();
        urlListPtr.release();
        pStateHandler->HandleResponse( status, pMessage, response, urlList );
        pUserHandler->HandleResponse( status, response, urlList );
      }

    private:
      XrdClient::FileStateHandler *pStateHandler;
      XrdClient::ResponseHandler  *pUserHandler;
      XrdClient::Message          *pMessage;
  };
}

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  FileStateHandler::FileStateHandler():
    pFileState( Closed ),
    pStatInfo( 0 ),
    pFileUrl( 0 ),
    pDataServer( 0 ),
    pLoadBalancer( 0 ),
    pFileHandle( 0 )
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  FileStateHandler::~FileStateHandler()
  {
    delete pStatInfo;
    delete pFileUrl;
    delete pDataServer;
    delete pLoadBalancer;
    delete [] pFileHandle;
  }

  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Open( const std::string &url,
                                       uint16_t           flags,
                                       uint16_t           mode,
                                       ResponseHandler   *handler,
                                       uint16_t           timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Check if we can proceed
    //--------------------------------------------------------------------------
    if( pFileState == Error )
      return pStatus;

    if( pFileState == OpenInProgress )
      return XRootDStatus( stError, errInProgress );

    if( pFileState == CloseInProgress || pFileState == Opened )
      return XRootDStatus( stError, errInvalidOp );

    pStatus = OpenInProgress;

    //--------------------------------------------------------------------------
    // Check if the parameters are valid
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    pFileUrl = new URL( url );
    if( !pFileUrl->IsValid() )
    {
      log->Error( FileMsg, "Trying to open invalid url: %s",
                             url.c_str() );
      pStatus    = XRootDStatus( stError, errInvalidArgs );
      pFileState = Error;
      return pStatus;
    }

    //--------------------------------------------------------------------------
    // Open the file
    //--------------------------------------------------------------------------
    log->Dump( FileMsg, "[%s] Sending a kXR_open request for path %s",
                         pFileUrl->GetHostId().c_str(),
                         pFileUrl->GetPathWithParams().c_str() );

    Message           *msg;
    ClientOpenRequest *req;
    std::string        path = pFileUrl->GetPathWithParams();
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_open;
    req->mode      = mode;
    req->options   = flags | kXR_async | kXR_retstat;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    OpenHandler *openHandler = new OpenHandler( this, handler );
    Status st = MessageUtils::SendMessage( *pFileUrl, msg, openHandler,
                                           timeout );

    if( !st.IsOK() )
    {
      pStatus    = st;
      pFileState = Error;
      return st;
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Close the file object
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Close( ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Check if we can proceed
    //--------------------------------------------------------------------------
    if( pFileState == Error )
      return pStatus;

    if( pFileState == CloseInProgress )
      return XRootDStatus( stError, errInProgress );

    if( pFileState == OpenInProgress || pFileState == Closed )
      return XRootDStatus( stError, errInvalidOp );

    pStatus = CloseInProgress;

    //--------------------------------------------------------------------------
    // Close the file
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[%s] Sending a kXR_close request for file handle "
                         "0x%x",
                         pFileUrl->GetHostId().c_str(),
                         *((uint32_t*)pFileHandle) );

    Message            *msg;
    ClientCloseRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_close;
    memcpy( req->fhandle, pFileHandle, 4 );

    CloseHandler *closeHandler = new CloseHandler( this, handler );
    Status st = MessageUtils::SendMessage( *pDataServer, msg, closeHandler,
                                           timeout );

    if( !st.IsOK() )
    {
      pStatus    = st;
      pFileState = Error;
      return st;
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Stat the file
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Stat( bool             force,
                                       ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened )
      return XRootDStatus( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Return the cached info
    //--------------------------------------------------------------------------
    if( !force )
    {
      AnyObject *obj = new AnyObject();
      obj->Set( new StatInfo( *pStatInfo ) );
      handler->HandleResponse( new XRootDStatus(),
                               obj,
                               new ResponseHandler::URLList() );
      return XRootDStatus();
    }

    //--------------------------------------------------------------------------
    // Issue a new stat request
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_stat request for file handle 0x%x",
                         pDataServer->GetHostId().c_str(),
                         *((uint32_t*)pFileHandle) );

    // stating a file handle doesn't work (fixed in 3.2.0) so we need to
    // stat the path
    Message           *msg;
    ClientStatRequest *req;
    std::string        path = pFileUrl->GetPath();
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_stat;
    req->dlen = path.length();
    msg->Append( path.c_str(), req->dlen, 24 );

    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg );
    Status st = MessageUtils::SendMessage( *pDataServer, msg, stHandler,
                                           timeout, false );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Read a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Read( uint64_t         offset,
                                       uint32_t         size,
                                       void            *buffer,
                                       ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_read request of %d bytes at %ld "
                         "offset for file handle 0x%x",
                         pDataServer->GetHostId().c_str(),
                         size, offset, *((uint32_t*)pFileHandle) );

    Message           *msg;
    ClientReadRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_read;
    req->offset     = offset;
    req->rlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );

    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg );
    Status st = MessageUtils::SendMessage( *pDataServer, msg, stHandler,
                                           timeout, false, (char*)buffer, size );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Write a data chank at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Write( uint64_t         offset,
                                        uint32_t         size,
                                        void            *buffer,
                                        ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_write request of %d bytes at %ld "
                         "offset for file handle 0x%x",
                         pDataServer->GetHostId().c_str(),
                         size, offset, *((uint32_t*)pFileHandle) );

    Message            *msg;
    ClientWriteRequest *req;
    MessageUtils::CreateRequest( msg, req, size );

    req->requestid  = kXR_write;
    req->offset     = offset;
    req->dlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );
    msg->Append( (char*)buffer, size, 24 );

    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg );
    Status st = MessageUtils::SendMessage( *pDataServer, msg, stHandler,
                                           timeout, false );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Commit all pending disk writes - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Sync( ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_sync request for file handle 0x%x",
                         pDataServer->GetHostId().c_str(),
                         *((uint32_t*)pFileHandle) );

    Message           *msg;
    ClientSyncRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_sync;
    memcpy( req->fhandle, pFileHandle, 4 );

    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg );
    Status st = MessageUtils::SendMessage( *pDataServer, msg, stHandler,
                                           timeout, false );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Truncate the file to a particular size - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Truncate( uint64_t         size,
                                           ResponseHandler *handler,
                                           uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_truncate request of size: %ld for "
                         "file handle 0x%x",
                         pDataServer->GetHostId().c_str(),
                         size, *((uint32_t*)pFileHandle) );

    Message               *msg;
    ClientTruncateRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_truncate;
    memcpy( req->fhandle, pFileHandle, 4 );
    req->offset = size;

    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg );
    Status st = MessageUtils::SendMessage( *pDataServer, msg, stHandler,
                                           timeout, false );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Read scattered data chunks in one operation - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::VectorRead( const ChunkList &chunks,
                                             void            *buffer,
                                             ResponseHandler *handler,
                                             uint16_t         timeout )
  {
    //--------------------------------------------------------------------------
    // Sanity check
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_readv request of %d chunks "
                         "for file handle 0x%x",
                         pDataServer->GetHostId().c_str(),
                         chunks.size(), *((uint32_t*)pFileHandle) );

    //--------------------------------------------------------------------------
    // Build the message
    //--------------------------------------------------------------------------
    Message            *msg;
    ClientReadVRequest *req;
    MessageUtils::CreateRequest( msg, req, sizeof(readahead_list)*chunks.size() );

    req->requestid = kXR_readv;
    req->dlen      = sizeof(readahead_list)*chunks.size();

    uint32_t size  = 0;
    //--------------------------------------------------------------------------
    // Copy the chunk info
    //--------------------------------------------------------------------------
    readahead_list *dataChunk = (readahead_list*)msg->GetBuffer( 24 );
    for( size_t i = 0; i < chunks.size(); ++i )
    {
      dataChunk[i].rlen   = chunks[i].length;
      dataChunk[i].offset = chunks[i].offset;
      memcpy( dataChunk[i].fhandle, pFileHandle, 4 );
      size       += chunks[i].length;
    }

    //--------------------------------------------------------------------------
    // Send the message
    //--------------------------------------------------------------------------
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg );
    Status st = MessageUtils::SendMessage( *pDataServer, msg, stHandler,
                                           timeout, false, (char*)buffer, size );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Process the results of the opening operation
  //----------------------------------------------------------------------------
  void FileStateHandler::SetOpenStatus(
                           const XRootDStatus             *status,
                           const OpenInfo                 *openInfo,
                           const ResponseHandler::URLList *hostList )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // FIXME: we need to check all the servers to properly assign the
    // load balancer, but it will do for now
    //--------------------------------------------------------------------------
    pDataServer   = new URL( hostList->back() );
    pLoadBalancer = new URL( hostList->front() );

    //--------------------------------------------------------------------------
    // We have failed
    //--------------------------------------------------------------------------
    pStatus = *status;
    if( !pStatus.IsOK() )
    {
      pFileState = Error;
      log->Debug( FileMsg, "[%s] Error opening file %s: %s",
                            pDataServer->GetHostId().c_str(),
                            pFileUrl->GetPath().c_str(),
                            pStatus.ToStr().c_str() );
    }
    //--------------------------------------------------------------------------
    // We have succeeded
    //--------------------------------------------------------------------------
    else
    {
      pFileState  = Opened;
      pFileHandle = new uint8_t[4];
      openInfo->GetFileHandle( pFileHandle );
      pStatInfo = new StatInfo( *openInfo->GetStatInfo() );
      log->Debug( FileMsg, "[%s] File %s successfully opened with id 0x%x",
                            pDataServer->GetHostId().c_str(),
                            pFileUrl->GetPath().c_str(),
                            *((uint32_t*)pFileHandle) );
    }
  }

  //----------------------------------------------------------------------------
  // Process the results of the closing operation
  //----------------------------------------------------------------------------
  void FileStateHandler::SetCloseStatus( const XRootDStatus *status )
  {
    pStatus    = *status;
    pFileState = Closed;
  }

  //----------------------------------------------------------------------------
  // Handle an error while sending a stateful message
  //----------------------------------------------------------------------------
  void FileStateHandler::HandleStateError( XRootDStatus    */*status*/,
                                           Message         */*message*/,
                                           ResponseHandler */*userHandler*/ )
  {
  }

  //----------------------------------------------------------------------------
  // Handle stateful redirect
  //----------------------------------------------------------------------------
  void FileStateHandler::HandleRedirection( URL             */*targetUrl*/,
                                            Message         */*message*/,
                                            ResponseHandler */*userHandler*/ )
  {
  }

  //----------------------------------------------------------------------------
  // Handle stateful response
  //----------------------------------------------------------------------------
  void FileStateHandler::HandleResponse( XRootDStatus             *status,
                                         Message                  *message,
                                         AnyObject                *response,
                                         ResponseHandler::URLList */*urlList*/ )
  {
    XRootDTransport::UnMarshallRequest( message );
    ClientRequest *req = (ClientRequest*)message->GetBuffer();

    if( !status->IsOK() )
      return;

    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // Cache the stat response
      //------------------------------------------------------------------------
      case kXR_stat:
      {
        StatInfo *info = 0;
        response->Get( info );
        pStatInfo = new StatInfo( *info );
      }
    };
  }
}
