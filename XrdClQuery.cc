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
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_locate request for path %s",
                         pUrl->GetHostId().c_str(), path.c_str() );
    Message *msg = new Message( sizeof( ClientLocateRequest )+path.length() );
    ClientLocateRequest *req = (ClientLocateRequest*)msg->GetBuffer();
    msg->Zero();

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

    handler.WaitForResponse();

    std::auto_ptr<AnyObject> resp( handler.GetResponse() );
    XRootDStatus *status = handler.GetStatus();
    XRootDStatus ret( *status );
    delete status;

    if( ret.IsOK() )
    {
      if( !resp.get() )
        return XRootDStatus( stError, errInternal );
      resp->Get( response );
      if( !response )
        return XRootDStatus( stError, errInternal );
    }

    resp->Set( (int *)0 );
    return ret;
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

    int size = sizeof( ClientMvRequest )+source.length()+dest.length()+1;
    Message *msg = new Message( size );
    ClientMvRequest *req = (ClientMvRequest*)msg->GetBuffer();
    msg->Zero();

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

    handler.WaitForResponse();

    XRootDStatus *status = handler.GetStatus();
    XRootDStatus ret( *status );
    delete status;
    return ret;
  }

  //----------------------------------------------------------------------------
  // Obtain server information - async
  //----------------------------------------------------------------------------
  XRootDStatus Query::ServerQuery( QueryCode::Code  queryCode,
                                   const Buffer    &arg,
                                   ResponseHandler *handler,
                                   uint16_t         timeout )
  {
    Log    *log = DefaultEnv::GetLog();
    log->Dump( QueryMsg, "[%s] Sending a kXR_query request [%d]",
                         pUrl->GetHostId().c_str(), queryCode );

    int headerSize = sizeof( ClientQueryRequest );
    Message *msg = new Message( headerSize+arg.GetSize() );
    ClientQueryRequest *req = (ClientQueryRequest*)msg->GetBuffer();
    msg->Zero();

    req->requestid = kXR_query;
    req->infotype  = queryCode;
    req->dlen      = arg.GetSize();
    memcpy( msg->GetBuffer(headerSize), arg.GetBuffer(), arg.GetSize() );

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

    handler.WaitForResponse();

    std::auto_ptr<AnyObject> resp( handler.GetResponse() );
    XRootDStatus *status = handler.GetStatus();
    XRootDStatus ret( *status );
    delete status;

    if( ret.IsOK() )
    {
      if( !resp.get() )
        return XRootDStatus( stError, errInternal );
      resp->Get( response );
      if( !response )
        return XRootDStatus( stError, errInternal );
    }

    resp->Set( (int *)0 );
    return ret;
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
