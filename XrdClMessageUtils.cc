//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Send a message
  //----------------------------------------------------------------------------
  Status MessageUtils::SendMessage( const URL       &url,
                                    Message         *msg,
                                    ResponseHandler *handler,
                                    uint16_t         timeout )
  {
    //--------------------------------------------------------------------------
    // Get the stuff needed to send the message
    //--------------------------------------------------------------------------
    Log        *log        = DefaultEnv::GetLog();
    PostMaster *postMaster = DefaultEnv::GetPostMaster();
    Status      st;

    AnyObject   sidMgrObj;
    SIDManager *sidMgr    = 0;
    st = postMaster->QueryTransport( url, XRootDQuery::SIDManager,
                                     sidMgrObj );

    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Unable to get stream id manager",
                             url.GetHostId().c_str() );
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
      log->Error( XRootDMsg, "[%s] Unable to allocate stream id",
                             url.GetHostId().c_str() );
      return st;
    }

    XRootDTransport::MarshallRequest( msg );

    //--------------------------------------------------------------------------
    // Create the message handler and send the thing into the wild
    //--------------------------------------------------------------------------
    XRootDMsgHandler *msgHandler;
    msgHandler = new XRootDMsgHandler( msg, handler, &url, sidMgr );
    msgHandler->SetTimeout( timeout );

    st = postMaster->Send( url, msg, msgHandler, 300 );
    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Unable to send the message 0x%x",
                             url.GetHostId().c_str(), &msg );
      delete msgHandler;
      return st;
    }
    return Status();
  }
}
