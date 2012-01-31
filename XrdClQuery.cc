//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClQuery.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <memory>

namespace
{
  //----------------------------------------------------------------------------
  // Synchronize the response
  //----------------------------------------------------------------------------
  class SyncResponseHandler: public XrdClient::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      SyncResponseHandler(): pStatus(0), pResponse(0), pSem(0) {}

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdClient::XRootDStatus *status,
                                   XrdClient::AnyObject    *response )
      {
        pStatus = status;
        pResponse = response;
        pSem.Post();
      }

      //------------------------------------------------------------------------
      // Get the status
      //------------------------------------------------------------------------
      XrdClient::XRootDStatus *GetStatus()
      {
        return pStatus;
      }

      //------------------------------------------------------------------------
      // Get the response
      //------------------------------------------------------------------------
      XrdClient::AnyObject *GetResponse()
      {
        return pResponse;
      }

      //------------------------------------------------------------------------
      // Wait for the arrival of the response
      //------------------------------------------------------------------------
      void WaitForResponse()
      {
        pSem.Wait();
      }

    private:
      XrdClient::XRootDStatus *pStatus;
      XrdClient::AnyObject    *pResponse;
      XrdSysSemaphore          pSem;
  };

  //----------------------------------------------------------------------------
  // Deep locate handler
  //----------------------------------------------------------------------------
  class DeepLocateHandler: public XrdClient::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      DeepLocateHandler( XrdClient::ResponseHandler *handler,
                         const std::string          &path,
                         uint16_t                    flags ):
        pFirstTime( true ),
        pOutstanding( 1 ),
        pHandler( handler ),
        pPath( path ),
        pFlags( flags )
      {
        pLocations = new XrdClient::LocationInfo();
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdClient::XRootDStatus *status,
                                   XrdClient::AnyObject    *response )
      {
        using namespace XrdClient;
        Log *log = DefaultEnv::GetLog();
        --pOutstanding;

        //----------------------------------------------------------------------
        // We've got an error, react accordingly
        //----------------------------------------------------------------------
        if( !status->IsOK() )
        {
          log->Dump( QueryMsg, "[DeepLocate] Got error response" );

          //--------------------------------------------------------------------
          // We have faile with the first request
          //--------------------------------------------------------------------
          if( pFirstTime )
          {
            log->Debug( QueryMsg, "[DeepLocate] Failed to get the initial "
                                  "location list" );
            pHandler->HandleResponse( status, response );
            return;
          }

          //--------------------------------------------------------------------
          // We have no more outstanding requests, so let give to the client
          // what we have
          //--------------------------------------------------------------------
          if( !pOutstanding )
          {
            log->Debug( QueryMsg, "[DeepLocate] No outstanding requests, "
                                  "give out what we've got" );
            HandleResponse();
          }

          return;
        }
        pFirstTime = false;

        //----------------------------------------------------------------------
        // Extract the answer
        //----------------------------------------------------------------------
        LocationInfo *info = 0;
        response->Get( info );
        LocationInfo::LocationIterator it;

        log->Dump( QueryMsg, "[DeepLocate] Got %d locations",
                             info->GetSize() );

        for( it = info->Begin(); it != info->End(); ++it )
        {
          //--------------------------------------------------------------------
          // Add the location to the list
          //--------------------------------------------------------------------
          if( it->IsServer() )
          {
            pLocations->Add( *it );
            continue;
          }

          //--------------------------------------------------------------------
          // Ask the manager for the location of servers
          //--------------------------------------------------------------------
          if( it->IsManager() )
          {
            Query q( it->GetAddress() );
            //!! FIXME timeout
            if( q.Locate( pPath, pFlags, this, 300 ).IsOK() )
              ++pOutstanding;
          }
        }

        //----------------------------------------------------------------------
        // Clean up and check if we have anything else to do
        //----------------------------------------------------------------------
        delete response;
        delete status;
        if( !pOutstanding )
          HandleResponse();
      }

      //------------------------------------------------------------------------
      // Build the response for the client
      //------------------------------------------------------------------------
      void HandleResponse()
      {
        using namespace XrdClient;

        //----------------------------------------------------------------------
        // Nothing found
        //----------------------------------------------------------------------
        if( !pLocations->GetSize() )
        {
          delete pLocations;
          pHandler->HandleResponse( new XRootDStatus( stError, errErrorResponse,
                                                  kXR_NotFound,
                                                  "No valid location found" ),
                                    0 );
        }
        //----------------------------------------------------------------------
        // We return an answer
        //----------------------------------------------------------------------
        else
        {
          AnyObject *obj = new AnyObject();
          obj->Set( pLocations );
          pHandler->HandleResponse( new XRootDStatus(), obj );
        }
        delete this;
      }

    private:
      bool                        pFirstTime;
      uint16_t                    pOutstanding;
      XrdClient::ResponseHandler *pHandler;
      XrdClient::LocationInfo    *pLocations;
      std::string                 pPath;
      uint16_t                    pFlags;
  };

  //----------------------------------------------------------------------------
  // Wait and return the status of the query
  //----------------------------------------------------------------------------
  XrdClient::XRootDStatus WaitForStatus( SyncResponseHandler *handler )
  {
    handler->WaitForResponse();
    XrdClient::XRootDStatus *status = handler->GetStatus();
    XrdClient::XRootDStatus ret( *status );
    delete status;
    return ret;
  }

  //----------------------------------------------------------------------------
  // Wait for the response
  //----------------------------------------------------------------------------
  template<class Type>
  XrdClient::XRootDStatus WaitForResponse( SyncResponseHandler  *handler,
                                           Type                *&response )
  {
    using namespace XrdClient;
    handler->WaitForResponse();

    std::auto_ptr<AnyObject> resp( handler->GetResponse() );
    XRootDStatus *status = handler->GetStatus();
    XRootDStatus ret( *status );
    delete status;

    if( ret.IsOK() )
    {
      if( !resp.get() )
        return XRootDStatus( stError, errInternal );
      resp->Get( response );
      resp->Set( (int *)0 );
      if( !response )
        return XRootDStatus( stError, errInternal );
    }

    return ret;
  }

  //----------------------------------------------------------------------------
  // Create a message
  //----------------------------------------------------------------------------
  template<class Type>
  void CreateRequest( XrdClient::Message *&msg, Type *&req,
                      uint32_t payloadSize = 0 )
  {
    using namespace XrdClient;
    msg = new Message( sizeof(Type)+payloadSize );
    req = (Type*)msg->GetBuffer();
    msg->Zero();
  }
}

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  Query::Query( const URL &url, PostMaster *postMaster )
  {
    if( postMaster )
      pPostMaster = postMaster;
    else
      pPostMaster = DefaultEnv::GetPostMaster();

    pUrl = new URL( url.GetURL() );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Query::~Query()
  {
    delete pUrl;
  }

  //----------------------------------------------------------------------------
  // Locate a file - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::Locate( const std::string &path,
                              uint16_t           flags,
                              ResponseHandler   *handler,
                              uint16_t           timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_locate request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );

    Message             *msg;
    ClientLocateRequest *req;
    CreateRequest( msg, req, path.length() );

    req->requestid = kXR_locate;
    req->options   = flags;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Locate a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::Locate( const std::string  &path,
                              uint16_t            flags,
                              LocationInfo      *&response,
                              uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = Locate( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Locate a file, recursively locate all disk servers - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::DeepLocate( const std::string &path,
                                  uint16_t           flags,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    return Locate( path, flags, new DeepLocateHandler( handler, path, flags ), timeout );
  }

  //----------------------------------------------------------------------------
  // Locate a file, recursively locate all disk servers - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::DeepLocate( const std::string  &path,
                                  uint16_t            flags,
                                  LocationInfo      *&response,
                                  uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = DeepLocate( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Move a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::Mv( const std::string &source,
                          const std::string &dest,
                          ResponseHandler   *handler,
                          uint16_t           timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_mv request to move %s to %s",
                         pUrl->GetHostId().c_str(),
                         source.c_str(), dest.c_str() );

    Message         *msg;
    ClientMvRequest *req;
    CreateRequest( msg, req, source.length()+dest.length()+1 );

    req->requestid = kXR_mv;
    req->dlen      = source.length()+dest.length()+1;
    msg->Append( source.c_str(), source.length(), 24 );
    *msg->GetBuffer(24+source.length()) = ' ';
    msg->Append( dest.c_str(), dest.length(), 25+source.length() );
    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Move a directory or a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::Mv( const std::string &source,
                          const std::string &dest,
                          uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Mv( source, dest, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain server information - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::ServerQuery( QueryCode::Code  queryCode,
                                   const Buffer    &arg,
                                   ResponseHandler *handler,
                                   uint16_t         timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_query request [%d]",
                         pUrl->GetHostId().c_str(), queryCode );

    Message            *msg;
    ClientQueryRequest *req;
    CreateRequest( msg, req, arg.GetSize() );

    req->requestid = kXR_query;
    req->infotype  = queryCode;
    req->dlen      = arg.GetSize();
    msg->Append( arg.GetBuffer(), arg.GetSize(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Obtain server information - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::ServerQuery( QueryCode::Code   queryCode,
                                   const Buffer     &arg,
                                   Buffer          *&response,
                                   uint16_t          timeout )
  {
    SyncResponseHandler handler;
    Status st = ServerQuery( queryCode, arg, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Truncate a file - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::Truncate( const std::string &path,
                                uint64_t           size,
                                ResponseHandler   *handler,
                                uint16_t           timeout )
  {
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_truncate request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );

    Message               *msg;
    ClientTruncateRequest *req;
    CreateRequest( msg, req, path.length() );

    req->requestid = kXR_truncate;
    req->offset    = size;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Truncate a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::Truncate( const std::string &path,
                                uint64_t           size,
                                uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Truncate( path, size, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Remove a file - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::Rm( const std::string &path,
                          ResponseHandler   *handler,
                          uint16_t           timeout )
  {
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_rm request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );

    Message         *msg;
    ClientRmRequest *req;
    CreateRequest( msg, req, path.length() );

    req->requestid = kXR_rm;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Remove a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::Rm( const std::string &path,
                          uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Rm( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Create a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::MkDir( const std::string &path,
                             uint8_t            flags,
                             uint16_t           mode,
                             ResponseHandler   *handler,
                             uint16_t           timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_mkdir request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );

    Message            *msg;
    ClientMkdirRequest *req;
    CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_mkdir;
    req->options[0] = flags;
    req->mode       = mode;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Create a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::MkDir( const std::string &path,
                             uint8_t            flags,
                             uint16_t           mode,
                             uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = MkDir( path, flags, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Remove a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::RmDir( const std::string &path,
                             ResponseHandler   *handler,
                             uint16_t           timeout )
  {
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_rmdir request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );

    Message            *msg;
    ClientRmdirRequest *req;
    CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_rmdir;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Remove a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::RmDir( const std::string &path,
                             uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = RmDir( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Change access mode on a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::ChMod( const std::string &path,
                             uint16_t           mode,
                             ResponseHandler   *handler,
                             uint16_t           timeout )
  {
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_chmod request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );

    Message            *msg;
    ClientChmodRequest *req;
    CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_chmod;
    req->mode       = mode;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Change access mode on a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::ChMod( const std::string &path,
                             uint16_t           mode,
                             uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = ChMod( path, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::Ping( ResponseHandler *handler,
                             uint16_t        timeout )
  {
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_ping request",
                         pUrl->GetHostId().c_str() );

    Message           *msg;
    ClientPingRequest *req;
    CreateRequest( msg, req );

    req->requestid  = kXR_ping;

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::Ping( uint16_t timeout  )
  {
    SyncResponseHandler handler;
    Status st = Ping( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::Stat( const std::string &path,
                            uint8_t            flags,
                            ResponseHandler   *handler,
                            uint16_t           timeout )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_stat request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );

    Message           *msg;
    ClientStatRequest *req;
    CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_stat;
    req->options    = flags;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::Stat( const std::string  &path,
                            uint16_t            flags,
                            StatInfo          *&response,
                            uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = Stat( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Obtain server protocol information - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::Protocol( ResponseHandler *handler,
                                uint16_t         timeout )
  {
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_protocol",
                         pUrl->GetHostId().c_str() );

    Message               *msg;
    ClientProtocolRequest *req;
    CreateRequest( msg, req );

    req->requestid = kXR_protocol;
    req->clientpv  = kXR_PROTOCOLVERSION;

    Status st = SendMessage( msg, handler, timeout );

    if( !st.IsOK() )
      return st;

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Obtain server protocol information - sync
  //----------------------------------------------------------------------------
  XRootDStatus Query::Protocol( ProtocolInfo *&response,
                                uint16_t       timeout )
  {
    SyncResponseHandler handler;
    Status st = Protocol( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Send a message and wait for a response
  //----------------------------------------------------------------------------
  Status Query::SendMessage( Message         *msg,
                             ResponseHandler *handler,
                             uint16_t         timeout )
  {
    //--------------------------------------------------------------------------
    // Get the stuff needed to send the message
    //--------------------------------------------------------------------------
    Log    *log = DefaultEnv::GetLog();
    Status  st;

    if( !pPostMaster )
    {
      log->Error( QueryMsg, "No post master object to handle the message" );
      return Status( stFatal, errConfig );
    }

    AnyObject   sidMgrObj;
    SIDManager *sidMgr    = 0;
    st = pPostMaster->QueryTransport( *pUrl, XRootDQuery::SIDManager,
                                      sidMgrObj );

    if( !st.IsOK() )
    {
      log->Error( QueryMsg, "[%s] Unable to get stream id manager",
                            pUrl->GetHostId().c_str() );
      return st;
    }
    sidMgrObj.Get( sidMgr );

    ClientLocateRequest *req = (ClientLocateRequest*)msg->GetBuffer();

    //--------------------------------------------------------------------------
    // Allocate the SID and marshall the message
    //--------------------------------------------------------------------------
    st = sidMgr->AllocateSID( req->streamid );
    if( !st.IsOK() )
    {
      log->Error( QueryMsg, "[%s] Unable to allocate stream id",
                            pUrl->GetHostId().c_str() );
      return st;
    }

    XRootDTransport::MarshallRequest( msg );

    //--------------------------------------------------------------------------
    // Create the message handler and send the thing into the wild
    //--------------------------------------------------------------------------
    XRootDMsgHandler *msgHandler = new XRootDMsgHandler( msg, handler, pUrl,
                                                         pPostMaster, sidMgr,
                                                         timeout );
    st = pPostMaster->Send( *pUrl, msg, msgHandler, 300 );
    if( !st.IsOK() )
    {
      log->Error( QueryMsg, "[%s] Unable to send the message 0x%x",
                            pUrl->GetHostId().c_str(), &msg );
      delete msgHandler;
      return st;
    }
    return Status();
  }
}
