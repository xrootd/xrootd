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
        pUserHandler->HandleResponse( status, response, urlList );
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

}

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  FileStateHandler::FileStateHandler():
    pFileState( Closed ),
    pStatInfo( 0 ),
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
    if( pFileState == Opened || pFileState == Error )
      return pStatus;

    if( pFileState == OpenInProgress )
      return XRootDStatus( stError, errInProgress );

    if( pFileState == CloseInProgress )
      return XRootDStatus( stError, errInvalidOp );

    pStatus = OpenInProgress;

    //--------------------------------------------------------------------------
    // Check if the parameters are valid
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    pFileUrl = new URL( url );
    if( !pFileUrl->IsValid() )
    {
      log->Error( QueryMsg, "Trying to open invalid url: %s",
                             url.c_str() );
      pStatus    = XRootDStatus( stError, errInvalidArgs );
      pFileState = Error;
      return pStatus;
    }

    //--------------------------------------------------------------------------
    // Open the file
    //--------------------------------------------------------------------------
    log->Dump( QueryMsg, "[%s] Sending a kXR_open request for path %s",
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
    if( pFileState == Closed || pFileState == Error )
      return pStatus;

    if( pFileState == CloseInProgress )
      return XRootDStatus( stError, errInProgress );

    if( pFileState == OpenInProgress )
      return XRootDStatus( stError, errInvalidOp );

    pStatus = CloseInProgress;

    //--------------------------------------------------------------------------
    // Close the file
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_close request for file handle "
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
    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Process the results of the opening operation
  //------------------------------------------------------------------------
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
      log->Debug( QueryMsg, "[%s] Error opening file %s: %s",
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
      log->Debug( QueryMsg, "[%s] File %s successfully opened with id 0x%x",
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
}
