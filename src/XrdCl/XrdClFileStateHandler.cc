//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFileStateHandler.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClForkHandler.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClFileTimer.hh"
#include "XrdCl/XrdClResponseJob.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClUglyHacks.hh"

#include <sstream>
#include <memory>
#include <sys/time.h>

namespace
{
  //----------------------------------------------------------------------------
  // Object that does things to the FileStateHandler when kXR_open returns
  // and then calls the user handler
  //----------------------------------------------------------------------------
  class OpenHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      OpenHandler( XrdCl::FileStateHandler *stateHandler,
                   XrdCl::ResponseHandler  *userHandler ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler )
      {
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        using namespace XrdCl;

        //----------------------------------------------------------------------
        // Extract the statistics info
        //----------------------------------------------------------------------
        OpenInfo *openInfo = 0;
        if( status->IsOK() )
          response->Get( openInfo );

        //----------------------------------------------------------------------
        // Notify the state handler and the client and say bye bye
        //----------------------------------------------------------------------
        pStateHandler->OnOpen( status, openInfo, hostList );
        delete response;
        if( pUserHandler )
          pUserHandler->HandleResponseWithHosts( status, 0, hostList );
        else
        {
          delete status;
          delete hostList;
        }
        delete this;
      }

    private:
      XrdCl::FileStateHandler *pStateHandler;
      XrdCl::ResponseHandler  *pUserHandler;
  };

  //----------------------------------------------------------------------------
  // Object that does things to the FileStateHandler when kXR_close returns
  // and then calls the user handler
  //----------------------------------------------------------------------------
  class CloseHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      CloseHandler( XrdCl::FileStateHandler *stateHandler,
                    XrdCl::ResponseHandler  *userHandler,
                    XrdCl::Message          *message ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler ),
        pMessage( message )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~CloseHandler()
      {
        delete pMessage;
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        pStateHandler->OnClose( status );
        if( pUserHandler )
          pUserHandler->HandleResponseWithHosts( status, response, hostList );
        else
        {
          delete response;
          delete status;
          delete hostList;
        }

        delete this;
      }

    private:
      XrdCl::FileStateHandler *pStateHandler;
      XrdCl::ResponseHandler  *pUserHandler;
      XrdCl::Message          *pMessage;
  };

  //----------------------------------------------------------------------------
  // Stateful message handler
  //----------------------------------------------------------------------------
  class StatefulHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StatefulHandler( XrdCl::FileStateHandler        *stateHandler,
                       XrdCl::ResponseHandler         *userHandler,
                       XrdCl::Message                 *message,
                       const XrdCl::MessageSendParams &sendParams ):
        pStateHandler( stateHandler ),
        pUserHandler( userHandler ),
        pMessage( message ),
        pSendParams( sendParams )
      {
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~StatefulHandler()
      {
        delete pMessage;
        delete pSendParams.chunkList;
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XrdCl::XRootDStatus *status,
                                            XrdCl::AnyObject    *response,
                                            XrdCl::HostList     *hostList )
      {
        using namespace XrdCl;
        XRDCL_SMART_PTR_T<AnyObject>       responsePtr( response );
        pSendParams.hostList = hostList;

        //----------------------------------------------------------------------
        // Houston we have a problem...
        //----------------------------------------------------------------------
        if( !status->IsOK() )
        {
          pStateHandler->OnStateError( status, pMessage, this, pSendParams );
          return;
        }

        //----------------------------------------------------------------------
        // We're clear
        //----------------------------------------------------------------------
        responsePtr.release();
        pStateHandler->OnStateResponse( status, pMessage, response, hostList );
        pUserHandler->HandleResponseWithHosts( status, response, hostList );
        delete this;
      }

      //------------------------------------------------------------------------
      //! Get the user handler
      //------------------------------------------------------------------------
      XrdCl::ResponseHandler *GetUserHandler()
      {
        return pUserHandler;
      }

    private:
      XrdCl::FileStateHandler  *pStateHandler;
      XrdCl::ResponseHandler   *pUserHandler;
      XrdCl::Message           *pMessage;
      XrdCl::MessageSendParams  pSendParams;
  };
}

namespace XrdCl
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
    pStateRedirect( 0 ),
    pFileHandle( 0 ),
    pOpenMode( 0 ),
    pOpenFlags( 0 ),
    pSessionId( 0 ),
    pDoRecoverRead( true ),
    pDoRecoverWrite( true ),
    pFollowRedirects( true )
  {
    pFileHandle = new uint8_t[4];
    ResetMonitoringVars();
    DefaultEnv::GetForkHandler()->RegisterFileObject( this );
    DefaultEnv::GetFileTimer()->RegisterFileObject( this );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  FileStateHandler::~FileStateHandler()
  {
    if( DefaultEnv::GetFileTimer() )
      DefaultEnv::GetFileTimer()->UnRegisterFileObject( this );

    if( DefaultEnv::GetForkHandler() )
      DefaultEnv::GetForkHandler()->UnRegisterFileObject( this );

    if( pFileState != Closed && DefaultEnv::GetLog() )
    {
      XRootDStatus st;
      MonitorClose( &st );
      ResetMonitoringVars();
    }

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

    if( pFileState == CloseInProgress || pFileState == Opened ||
        pFileState == Recovering )
      return XRootDStatus( stError, errInvalidOp );

    pStatus = OpenInProgress;

    //--------------------------------------------------------------------------
    // Check if the parameters are valid
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    pFileUrl = new URL( url );
    if( !pFileUrl->IsValid() )
    {
      log->Error( FileMsg, "[0x%x@%s] Trying to open invalid url: %s",
                  this, pFileUrl->GetPath().c_str(), url.c_str() );
      pStatus    = XRootDStatus( stError, errInvalidArgs );
      pFileState = Error;
      return pStatus;
    }

    //--------------------------------------------------------------------------
    // Check if the recovery procedures should be enabled
    //--------------------------------------------------------------------------
    const URL::ParamsMap &urlParams = pFileUrl->GetParams();
    URL::ParamsMap::const_iterator it;
    it = urlParams.find( "xrdcl.recover-reads" );
    if( (it != urlParams.end() && it->second == "false") ||
        !pDoRecoverRead )
    {
      pDoRecoverRead = false;
      log->Debug( FileMsg, "[0x%x@%s] Read recovery procedures are disabled",
                  this, pFileUrl->GetURL().c_str() );
    }

    it = urlParams.find( "xrdcl.recover-writes" );
    if( (it != urlParams.end() && it->second == "false") ||
        !pDoRecoverWrite )
    {
      pDoRecoverWrite = false;
      log->Debug( FileMsg, "[0x%x@%s] Write recovery procedures are disabled",
                  this, pFileUrl->GetURL().c_str() );
    }

    //--------------------------------------------------------------------------
    // Open the file
    //--------------------------------------------------------------------------
    log->Debug( FileMsg, "[0x%x@%s] Sending an open command", this,
                pFileUrl->GetURL().c_str() );

    pOpenMode  = mode;
    pOpenFlags = flags;

    Message           *msg;
    ClientOpenRequest *req;
    std::string        path = pFileUrl->GetPathWithParams();
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_open;
    req->mode      = mode;
    req->options   = flags | kXR_async | kXR_retstat;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    XRootDTransport::SetDescription( msg );
    OpenHandler *openHandler = new OpenHandler( this, handler );
    MessageSendParams params; params.timeout = timeout;
    params.followRedirects = pFollowRedirects;
    MessageUtils::ProcessSendParams( params );

    Status st = MessageUtils::SendMessage( *pFileUrl, msg, openHandler, params );

    if( !st.IsOK() )
    {
      delete openHandler;
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

    if( pFileState == OpenInProgress || pFileState == Closed ||
        pFileState == Recovering || !pInTheFly.empty() )
      return XRootDStatus( stError, errInvalidOp );

    pStatus = CloseInProgress;

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a close command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Close the file
    //--------------------------------------------------------------------------
    Message            *msg;
    ClientCloseRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_close;
    memcpy( req->fhandle, pFileHandle, 4 );

    XRootDTransport::SetDescription( msg );
    msg->SetSessionId( pSessionId );
    CloseHandler *closeHandler = new CloseHandler( this, handler, msg );
    MessageSendParams params;
    params.timeout = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    Status st = MessageUtils::SendMessage( *pDataServer, msg, closeHandler, params );

    if( !st.IsOK() )
    {
      delete closeHandler;
      if( st.code == errInvalidSession && IsReadOnly() )
      {
        pFileState = Closed;
        return st;
      }

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

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Return the cached info
    //--------------------------------------------------------------------------
    if( !force )
    {
      AnyObject *obj = new AnyObject();
      obj->Set( new StatInfo( *pStatInfo ) );
      handler->HandleResponseWithHosts( new XRootDStatus(), obj,
                                        new HostList() );
      return XRootDStatus();
    }

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a stat command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Issue a new stat request
    // stating a file handle doesn't work (fixed in 3.2.0) so we need to
    // stat the pat
    //--------------------------------------------------------------------------
    Message           *msg;
    ClientStatRequest *req;
    std::string        path = pFileUrl->GetPath();
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_stat;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
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

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a read command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message           *msg;
    ClientReadRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_read;
    req->offset     = offset;
    req->rlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );

    ChunkList *list   = new ChunkList();
    list->push_back( ChunkInfo( offset, size, buffer ) );

    XRootDTransport::SetDescription( msg );
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    MessageUtils::ProcessSendParams( params );

    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Write( uint64_t         offset,
                                        uint32_t         size,
                                        const void      *buffer,
                                        ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a write command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientWriteRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_write;
    req->offset     = offset;
    req->dlen       = size;
    memcpy( req->fhandle, pFileHandle, 4 );

    ChunkList *list   = new ChunkList();
    list->push_back( ChunkInfo( 0, size, (char*)buffer ) );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;

    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Commit all pending disk writes - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Sync( ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a sync command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message           *msg;
    ClientSyncRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_sync;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Truncate the file to a particular size - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Truncate( uint64_t         size,
                                           ResponseHandler *handler,
                                           uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a truncate command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message               *msg;
    ClientTruncateRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_truncate;
    memcpy( req->fhandle, pFileHandle, 4 );
    req->offset = size;

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
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

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a vector read command for handle "
                "0x%x to %s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    //--------------------------------------------------------------------------
    // Build the message
    //--------------------------------------------------------------------------
    Message            *msg;
    ClientReadVRequest *req;
    MessageUtils::CreateRequest( msg, req, sizeof(readahead_list)*chunks.size() );

    req->requestid = kXR_readv;
    req->dlen      = sizeof(readahead_list)*chunks.size();

    ChunkList *list   = new ChunkList();
    char      *cursor = (char*)buffer;

    //--------------------------------------------------------------------------
    // Copy the chunk info
    //--------------------------------------------------------------------------
    readahead_list *dataChunk = (readahead_list*)msg->GetBuffer( 24 );
    for( size_t i = 0; i < chunks.size(); ++i )
    {
      dataChunk[i].rlen   = chunks[i].length;
      dataChunk[i].offset = chunks[i].offset;
      memcpy( dataChunk[i].fhandle, pFileHandle, 4 );

      void *chunkBuffer;
      if( cursor )
      {
        chunkBuffer  = cursor;
        cursor      += chunks[i].length;
      }
      else
        chunkBuffer = chunks[i].buffer;

      list->push_back( ChunkInfo( chunks[i].offset,
                                  chunks[i].length,
                                  chunkBuffer ) );
    }

    //--------------------------------------------------------------------------
    // Send the message
    //--------------------------------------------------------------------------
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    params.chunkList       = list;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Performs a custom operation on an open file, server implementation
  // dependent - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Fcntl( const Buffer    &arg,
                                        ResponseHandler *handler,
                                        uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a fcntl command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientQueryRequest *req;
    MessageUtils::CreateRequest( msg, req, arg.GetSize() );

    req->requestid = kXR_query;
    req->infotype  = kXR_Qopaqug;
    req->dlen      = arg.GetSize();
    memcpy( req->fhandle, pFileHandle, 4 );
    msg->Append( arg.GetBuffer(), arg.GetSize(), 24 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Get access token to a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileStateHandler::Visa( ResponseHandler *handler,
                                       uint16_t         timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState != Opened && pFileState != Recovering )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Sending a visa command for handle 0x%x to "
                "%s", this, pFileUrl->GetURL().c_str(),
                *((uint32_t*)pFileHandle), pDataServer->GetHostId().c_str() );

    Message            *msg;
    ClientQueryRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_query;
    req->infotype  = kXR_Qvisa;
    memcpy( req->fhandle, pFileHandle, 4 );

    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );
    StatefulHandler *stHandler = new StatefulHandler( this, handler, msg, params );
    return SendOrQueue( *pDataServer, msg, stHandler, params );
  }

  //----------------------------------------------------------------------------
  // Check if the file is open
  //----------------------------------------------------------------------------
  bool FileStateHandler::IsOpen() const
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pFileState == Opened || pFileState == Recovering )
      return true;
    return false;
  }

  //----------------------------------------------------------------------------
  // Set file property
  //----------------------------------------------------------------------------
  bool FileStateHandler::SetProperty( const std::string &name,
                                      const std::string &value )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    if( name == "ReadRecovery" )
    {
      if( value == "true" ) pDoRecoverRead = true;
      else pDoRecoverRead = false;
      return true;
    }
    else if( name == "WriteRecovery" )
    {
      if( value == "true" ) pDoRecoverWrite = true;
      else pDoRecoverWrite = false;
      return true;
    }
    else if( name == "FollowRedirects" )
    {
      if( value == "true" ) pFollowRedirects = true;
      else pFollowRedirects = false;
      return true;
    }
    return false;
  }

  //----------------------------------------------------------------------------
  // Get file property
  //----------------------------------------------------------------------------
  bool FileStateHandler::GetProperty( const std::string &name,
                                      std::string &value ) const
  {
    XrdSysMutexHelper scopedLock( pMutex );
    if( name == "ReadRecovery" )
    {
      if( pDoRecoverRead ) value = "true";
      else value = "false";
      return true;
    }
    else if( name == "WriteRecovery" )
    {
      if( pDoRecoverWrite ) value = "true";
      else value = "false";
      return true;
    }
    else if( name == "FollowRedirects" )
    {
      if( pFollowRedirects ) value = "true";
      else value = "false";
      return true;
    }
    else if( name == "DataServer" && pDataServer )
      { value = pDataServer->GetHostId(); return true; }
    else if( name == "LastURL" && pDataServer )
      { value =  pDataServer->GetURL(); return true; }
    value = "";
    return false;
  }

  //----------------------------------------------------------------------------
  // Process the results of the opening operation
  //----------------------------------------------------------------------------
  void FileStateHandler::OnOpen( const XRootDStatus *status,
                                 const OpenInfo     *openInfo,
                                 const HostList     *hostList )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Assign the data server and the load balancer
    //--------------------------------------------------------------------------
    std::string lastServer = pFileUrl->GetHostId();
    if( hostList )
    {
      delete pDataServer;
      delete pLoadBalancer;
      pLoadBalancer = 0;

      pDataServer = new URL( hostList->back().url );
      pDataServer->SetParams( pFileUrl->GetParams() );
      pDataServer->SetPath( pFileUrl->GetPath() );
      lastServer = pDataServer->GetHostId();
      HostList::const_iterator itC;
      URL::ParamsMap params = pDataServer->GetParams();
      for( itC = hostList->begin(); itC != hostList->end(); ++itC )
      {
        MessageUtils::MergeCGI( params,
                                itC->url.GetParams(),
                                true );
      }
      pDataServer->SetParams( params );

      HostList::const_reverse_iterator it;
      for( it = hostList->rbegin(); it != hostList->rend(); ++it )
        if( it->loadBalancer )
        {
          pLoadBalancer = new URL( it->url );
          break;
        }
    }

    log->Debug( FileMsg, "[0x%x@%s] Open has returned with status %s",
                this, pFileUrl->GetURL().c_str(), status->ToStr().c_str() );

    //--------------------------------------------------------------------------
    // We have failed
    //--------------------------------------------------------------------------
    pStatus = *status;
    if( !pStatus.IsOK() )
    {
      log->Debug( FileMsg, "[0x%x@%s] Error while opening at %s: %s",
                  this, pFileUrl->GetURL().c_str(), lastServer.c_str(),
                  pStatus.ToStr().c_str() );
      FailQueuedMessages( pStatus );
      pFileState = Error;

      //------------------------------------------------------------------------
      // Report to monitoring
      //------------------------------------------------------------------------
      Monitor *mon = DefaultEnv::GetMonitor();
      if( mon )
      {
        Monitor::ErrorInfo i;
        i.file   = pFileUrl;
        i.status = status;
        i.opCode = Monitor::ErrorInfo::ErrOpen;
        mon->Event( Monitor::EvErrIO, &i );
      }
    }
    //--------------------------------------------------------------------------
    // We have succeeded
    //--------------------------------------------------------------------------
    else
    {
      //------------------------------------------------------------------------
      // Store the response info
      //------------------------------------------------------------------------
      openInfo->GetFileHandle( pFileHandle );
      pSessionId = openInfo->GetSessionId();
      if( openInfo->GetStatInfo() )
      {
        delete pStatInfo;
        pStatInfo = new StatInfo( *openInfo->GetStatInfo() );
      }

      log->Debug( FileMsg, "[0x%x@%s] successfully opened at %s, handle: 0x%x, "
                  "session id: %ld", this, pFileUrl->GetURL().c_str(),
                  pDataServer->GetHostId().c_str(), *((uint32_t*)pFileHandle),
                  pSessionId );

      //------------------------------------------------------------------------
      // Inform the monitoring about opening success
      //------------------------------------------------------------------------
      gettimeofday( &pOpenTime, 0 );
      Monitor *mon = DefaultEnv::GetMonitor();
      if( mon )
      {
        Monitor::OpenInfo i;
        i.file       = pFileUrl;
        i.dataServer = pDataServer->GetHostId();
        i.oFlags     = pOpenFlags;
        i.fSize      = pStatInfo ? pStatInfo->GetSize() : 0;
        mon->Event( Monitor::EvOpen, &i );
      }

      //------------------------------------------------------------------------
      // Resend the queued messages if any
      //------------------------------------------------------------------------
      ReSendQueuedMessages();
      pFileState  = Opened;
    }
  }

  //----------------------------------------------------------------------------
  // Process the results of the closing operation
  //----------------------------------------------------------------------------
  void FileStateHandler::OnClose( const XRootDStatus *status )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    log->Debug( FileMsg, "[0x%x@%s] Close returned from %s with: %s", this,
                pFileUrl->GetURL().c_str(), pDataServer->GetHostId().c_str(),
                status->ToStr().c_str() );

    log->Dump( FileMsg, "[0x%x@%s] Items in the fly %d, queued for recovery %d",
               this, pFileUrl->GetURL().c_str(), pInTheFly.size(),
               pToBeRecovered.size() );

    MonitorClose( status );
    ResetMonitoringVars();

    pStatus    = *status;
    pFileState = Closed;
  }

  //----------------------------------------------------------------------------
  // Handle an error while sending a stateful message
  //----------------------------------------------------------------------------
  void FileStateHandler::OnStateError( XRootDStatus      *status,
                                       Message           *message,
                                       ResponseHandler   *userHandler,
                                       MessageSendParams &sendParams )
  {
    //--------------------------------------------------------------------------
    // It may be a redirection
    //--------------------------------------------------------------------------
    if( !status->IsOK() && status->code == errRedirect && pFollowRedirects )
    {
      std::string root = "root", xroot = "xroot";
      std::string msg = status->GetErrorMessage();
      if( !msg.compare( 0, root.size(), root ) ||
          !msg.compare( 0, xroot.size(), xroot ) )
      {
        OnStateRedirection( msg, message, userHandler, sendParams );
        return;
      }
    }

    //--------------------------------------------------------------------------
    // Handle error
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );
    pInTheFly.erase( message );

    log->Dump( FileMsg, "[0x%x@%s] File state error encountered. Message %s "
               "returned with %s", this, pFileUrl->GetURL().c_str(),
               message->GetDescription().c_str(), status->ToStr().c_str() );

    //--------------------------------------------------------------------------
    // Report to monitoring
    //--------------------------------------------------------------------------
    Monitor *mon = DefaultEnv::GetMonitor();
    if( mon )
    {
      Monitor::ErrorInfo i;
      i.file   = pFileUrl;
      i.status = status;

      ClientRequest *req = (ClientRequest*)message->GetBuffer();
      switch( req->header.requestid )
      {
        case kXR_read:  i.opCode = Monitor::ErrorInfo::ErrRead;  break;
        case kXR_readv: i.opCode = Monitor::ErrorInfo::ErrReadV; break;
        case kXR_write: i.opCode = Monitor::ErrorInfo::ErrWrite; break;
        default: i.opCode = Monitor::ErrorInfo::ErrUnc;
      }

      mon->Event( Monitor::EvErrIO, &i );
    }

    //--------------------------------------------------------------------------
    // The message is not recoverable
    //--------------------------------------------------------------------------
    if( !IsRecoverable( *status ) )
    {
      log->Error( FileMsg, "[0x%x@%s] Fatal file state error. Message %s "
                 "returned with %s", this, pFileUrl->GetURL().c_str(),
                 message->GetDescription().c_str(), status->ToStr().c_str() );

      FailMessage( RequestData( message, userHandler, sendParams ), *status );
      delete status;
      return;
    }

    //--------------------------------------------------------------------------
    // Insert the message to the recovery queue and start the recovery
    // procedure if we don't have any more message in the fly
    //--------------------------------------------------------------------------
    pCloseReason = *status;
    RecoverMessage( RequestData( message, userHandler, sendParams ) );
    delete status;
  }

  //----------------------------------------------------------------------------
  // Handle stateful redirect
  //----------------------------------------------------------------------------
  void FileStateHandler::OnStateRedirection( const std::string &redirectUrl,
                                             Message           *message,
                                             ResponseHandler   *userHandler,
                                             MessageSendParams &sendParams )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    pInTheFly.erase( message );

    //--------------------------------------------------------------------------
    // Register the state redirect url and append the new cgi information to
    // the file URL
    //--------------------------------------------------------------------------
    if( !pStateRedirect )
    {
      std::ostringstream o;
      pStateRedirect = new URL( redirectUrl );
      URL::ParamsMap params = pFileUrl->GetParams();
      MessageUtils::MergeCGI( params,
                              pStateRedirect->GetParams(),
                              false );
      pFileUrl->SetParams( params );
    }

    RecoverMessage( RequestData( message, userHandler, sendParams ) );
  }

  //----------------------------------------------------------------------------
  // Handle stateful response
  //----------------------------------------------------------------------------
  void FileStateHandler::OnStateResponse( XRootDStatus *status,
                                          Message      *message,
                                          AnyObject    *response,
                                          HostList     */*urlList*/ )
  {
    Log    *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    log->Dump( FileMsg, "[0x%x@%s] Got state response for message %s",
               this, pFileUrl->GetURL().c_str(),
               message->GetDescription().c_str() );

    //--------------------------------------------------------------------------
    // Since this message may be the last "in-the-fly" and no recovery
    // is done if messages are in the fly, we may need to trigger recovery
    //--------------------------------------------------------------------------
    pInTheFly.erase( message );
    RunRecovery();

    //--------------------------------------------------------------------------
    // Play with the actual response before returning it. This is a good
    // place to do caching in the future.
    //--------------------------------------------------------------------------
    ClientRequest *req = (ClientRequest*)message->GetBuffer();
    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // Cache the stat response
      //------------------------------------------------------------------------
      case kXR_stat:
      {
        StatInfo *info = 0;
        response->Get( info );
        delete pStatInfo;
        pStatInfo = new StatInfo( *info );
        break;
      }

      //------------------------------------------------------------------------
      // Handle read response
      //------------------------------------------------------------------------
      case kXR_read:
      {
        ++pRCount;
        pRBytes += req->read.rlen;
        break;
      }

      //------------------------------------------------------------------------
      // Handle readv response
      //------------------------------------------------------------------------
      case kXR_readv:
      {
        ++pVCount;
        size_t segs = req->header.dlen/sizeof(readahead_list);
        readahead_list *dataChunk = (readahead_list*)message->GetBuffer( 24 );
        for( size_t i = 0; i < segs; ++i )
          pVBytes += dataChunk[i].rlen;
        pVSegs += segs;
        break;
      }

      //------------------------------------------------------------------------
      // Handle write response
      //------------------------------------------------------------------------
      case kXR_write:
      {
        ++pWCount;
        pWBytes += req->write.dlen;
        break;
      }
    };
  }

  //------------------------------------------------------------------------
  //! Tick
  //------------------------------------------------------------------------
  void FileStateHandler::Tick( time_t now )
  {
    TimeOutRequests( now );
    XrdSysMutexHelper scopedLock( pMutex );
  }

  //----------------------------------------------------------------------------
  // Declare timeout on requests being recovered
  //----------------------------------------------------------------------------
  void FileStateHandler::TimeOutRequests( time_t now )
  {
    if( !pToBeRecovered.empty() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Dump( FileMsg, "[0x%x@%s] Got a timer event", this,
                 pFileUrl->GetURL().c_str() );
      RequestList::iterator it;
      JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
      for( it = pToBeRecovered.begin(); it != pToBeRecovered.end(); )
      {
        if( it->params.expires <= now )
        {
          jobMan->QueueJob( new ResponseJob(
                              it->handler,
                              new XRootDStatus( stError, errOperationExpired ),
                              0, it->params.hostList ) );
          it = pToBeRecovered.erase( it );
        }
        else
          ++it;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Called in the child process after the fork
  //----------------------------------------------------------------------------
  void FileStateHandler::AfterForkChild()
  {
    Log *log = DefaultEnv::GetLog();

    if( pFileState == Closed || pFileState == Error )
      return;

    if( (IsReadOnly() && pDoRecoverRead) ||
        (!IsReadOnly() && pDoRecoverWrite) )
    {
      log->Debug( FileMsg, "[0x%x@%s] Putting the file in recovery state in "
                  "process %d", this, pFileUrl->GetURL().c_str(), getpid() );
      pFileState = Recovering;
      pInTheFly.clear();
      pToBeRecovered.clear();
    }
    else
      pFileState = Error;
  }

  //----------------------------------------------------------------------------
  // Send a message to a host or put it in the recovery queue
  //----------------------------------------------------------------------------
  Status FileStateHandler::SendOrQueue( const URL         &url,
                                        Message           *msg,
                                        ResponseHandler   *handler,
                                        MessageSendParams &sendParams )
  {
    //--------------------------------------------------------------------------
    // Recovering
    //--------------------------------------------------------------------------
    if( pFileState == Recovering )
    {
      return RecoverMessage( RequestData( msg, handler, sendParams ), false );
    }

    //--------------------------------------------------------------------------
    // Trying to send
    //--------------------------------------------------------------------------
    if( pFileState == Opened )
    {
      msg->SetSessionId( pSessionId );
      Status st = MessageUtils::SendMessage( *pDataServer, msg, handler, sendParams );

      //------------------------------------------------------------------------
      // Invalid session id means that the connection has been broken while we
      // were idle so we haven't been informed about this fact earlier.
      //------------------------------------------------------------------------
      if( !st.IsOK() && st.code == errInvalidSession && IsRecoverable( st ) )
        return RecoverMessage( RequestData( msg, handler, sendParams ), false );

      if( st.IsOK() )
        pInTheFly.insert(msg);
      else
        delete handler;
      return st;
    }
    return Status( stError, errInvalidOp );
  }

  //----------------------------------------------------------------------------
  // Check if the stateful error is recoverable
  //----------------------------------------------------------------------------
  bool FileStateHandler::IsRecoverable( const XRootDStatus &status ) const
  {
    if( status.code == errSocketError || status.code == errInvalidSession )
    {
      if( IsReadOnly() && !pDoRecoverRead )
        return false;

      if( !IsReadOnly() && !pDoRecoverWrite )
        return false;

      return true;
    }
    return false;
  }

  //----------------------------------------------------------------------------
  // Check if the file is open for read only
  //----------------------------------------------------------------------------
  bool FileStateHandler::IsReadOnly() const
  {
    if( (pOpenFlags & kXR_open_read) && !(pOpenFlags & kXR_open_updt) &&
        !(pOpenFlags & kXR_open_apnd ) )
      return true;
    return false;
  }

  //----------------------------------------------------------------------------
  // Recover a message
  //----------------------------------------------------------------------------
  Status FileStateHandler::RecoverMessage( RequestData rd,
                                           bool        callbackOnFailure )
  {
    pFileState = Recovering;

    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Putting message %s in the recovery list",
               this, pFileUrl->GetURL().c_str(),
               rd.request->GetDescription().c_str() );

    Status st = RunRecovery();
    if( st.IsOK() )
    {
      pToBeRecovered.push_back( rd );
      return st;
    }

    if( callbackOnFailure )
      FailMessage( rd, st );

    return st;
  }

  //----------------------------------------------------------------------------
  // Run the recovery procedure if appropriate
  //----------------------------------------------------------------------------
  Status FileStateHandler::RunRecovery()
  {
    if( pFileState != Recovering )
      return Status();

    if( !pInTheFly.empty() )
      return Status();

    Log *log = DefaultEnv::GetLog();
    log->Debug( FileMsg, "[0x%x@%s] Running the recovery procedure", this,
                pFileUrl->GetURL().c_str() );

    Status st;
    if( pStateRedirect )
    {
      SendClose( 0 );
      st = ReOpenFileAtServer( *pStateRedirect, 0 );
      delete pStateRedirect; pStateRedirect = 0;
    }
    else if( IsReadOnly() && pLoadBalancer )
      st = ReOpenFileAtServer( *pLoadBalancer, 0 );
    else
      st = ReOpenFileAtServer( *pDataServer, 0 );

    if( !st.IsOK() )
    {
      pFileState = Error;
      FailQueuedMessages( st );
    }

    return st;
  }

  //----------------------------------------------------------------------------
  // Send a close and ignore the response
  //----------------------------------------------------------------------------
  Status FileStateHandler::SendClose( uint16_t timeout )
  {
    Message            *msg;
    ClientCloseRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_close;
    memcpy( req->fhandle, pFileHandle, 4 );

    XRootDTransport::SetDescription( msg );
    msg->SetSessionId( pSessionId );
    NullResponseHandler *handler = new NullResponseHandler();
    MessageSendParams params;
    params.timeout         = timeout;
    params.followRedirects = false;
    params.stateful        = true;

    MessageUtils::ProcessSendParams( params );

    return MessageUtils::SendMessage( *pDataServer, msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Re-open the current file at a given server
  //----------------------------------------------------------------------------
  Status FileStateHandler::ReOpenFileAtServer( const URL &url, uint16_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Sending a recovery open command to %s",
               this, pFileUrl->GetURL().c_str(), url.GetURL().c_str() );

    //--------------------------------------------------------------------------
    // Remove the kXR_delete and kXR_new flags, we don't want the recovery
    // procedure to delete a file that has been partially updated or fail
    // it because a partially uploaded file already exists
    //--------------------------------------------------------------------------
    pOpenFlags &= ~kXR_delete;
    pOpenFlags &= ~kXR_new;

    Message           *msg;
    ClientOpenRequest *req;

    URL u = *pFileUrl;
    if( !url.GetPath().empty() )
      u.SetPath( url.GetPath() );
    std::string        path = u.GetPathWithParams();
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_open;
    req->mode      = pOpenMode;
    req->options   = pOpenFlags;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    OpenHandler *openHandler = new OpenHandler( this, 0 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );
    Status st = MessageUtils::SendMessage( url, msg, openHandler, params );
    if( !st.IsOK() )
      delete openHandler;
    return st;
  }

  //------------------------------------------------------------------------
  // Fail a message
  //------------------------------------------------------------------------
  void FileStateHandler::FailMessage( RequestData rd, XRootDStatus status )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Failing message %s with %s",
               this, pFileUrl->GetURL().c_str(),
               rd.request->GetDescription().c_str(),
               status.ToStr().c_str() );

    StatefulHandler *sh = dynamic_cast<StatefulHandler*>(rd.handler);
    if( !sh )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "[0x%x@%s] Internal error while recovering %s",
                  this, pFileUrl->GetURL().c_str(),
                  rd.request->GetDescription().c_str() );
      return;
    }
    ResponseHandler *userHandler = sh->GetUserHandler();
    userHandler->HandleResponseWithHosts( new XRootDStatus( status ), 0,
                                          rd.params.hostList );
    delete sh;
  }

  //----------------------------------------------------------------------------
  // Fail queued messages
  //----------------------------------------------------------------------------
  void FileStateHandler::FailQueuedMessages( XRootDStatus status )
  {
    RequestList::iterator it;
    for( it = pToBeRecovered.begin(); it != pToBeRecovered.end(); ++it )
      FailMessage( *it, status );
    pToBeRecovered.clear();
  }

  //------------------------------------------------------------------------
  // Re-send queued messages
  //------------------------------------------------------------------------
  void FileStateHandler::ReSendQueuedMessages()
  {
    RequestList::iterator it;
    for( it = pToBeRecovered.begin(); it != pToBeRecovered.end(); ++it )
    {
      it->request->SetSessionId( pSessionId );
      ReWriteFileHandle( it->request );
      Status st = MessageUtils::SendMessage( *pDataServer, it->request,
                                             it->handler, it->params );
      if( !st.IsOK() )
        FailMessage( *it, st );
    }
    pToBeRecovered.clear();
  }

  //------------------------------------------------------------------------
  // Re-write file handle
  //------------------------------------------------------------------------
  void FileStateHandler::ReWriteFileHandle( Message *msg )
  {
    ClientRequestHdr *hdr = (ClientRequestHdr*)msg->GetBuffer();
    switch( hdr->requestid )
    {
      case kXR_read:
      {
        ClientReadRequest *req = (ClientReadRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_write:
      {
        ClientWriteRequest *req = (ClientWriteRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_sync:
      {
        ClientSyncRequest *req = (ClientSyncRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_truncate:
      {
        ClientTruncateRequest *req = (ClientTruncateRequest*)msg->GetBuffer();
        memcpy( req->fhandle, pFileHandle, 4 );
        break;
      }
      case kXR_readv:
      {
        ClientReadVRequest *req = (ClientReadVRequest*)msg->GetBuffer();
        readahead_list *dataChunk = (readahead_list*)msg->GetBuffer( 24 );
        for( size_t i = 0; i < req->dlen/sizeof(readahead_list); ++i )
          memcpy( dataChunk[i].fhandle, pFileHandle, 4 );
        break;
      }
    }

    Log *log = DefaultEnv::GetLog();
    log->Dump( FileMsg, "[0x%x@%s] Rewritten file handle for %s to 0x%x",
               this, pFileUrl->GetURL().c_str(), msg->GetDescription().c_str(),
               *((uint32_t*)pFileHandle) );
    XRootDTransport::SetDescription( msg );
  }

  //----------------------------------------------------------------------------
  // Dispatch monitoring information on close
  //----------------------------------------------------------------------------
  void FileStateHandler::MonitorClose( const XRootDStatus *status )
  {
    Monitor *mon = DefaultEnv::GetMonitor();
    if( mon )
    {
      Monitor::CloseInfo i;
      i.file = pFileUrl;
      i.oTOD = pOpenTime;
      gettimeofday( &i.cTOD, 0 );
      i.rBytes = pRBytes;
      i.vBytes = pVBytes;
      i.wBytes = pWBytes;
      i.vSegs  = pVSegs;
      i.rCount = pRCount;
      i.vCount = pVCount;
      i.wCount = pWCount;
      i.status = status;
      mon->Event( Monitor::EvClose, &i );
    }
  }
}
