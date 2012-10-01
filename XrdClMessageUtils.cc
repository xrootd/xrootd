//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Send a message
  //----------------------------------------------------------------------------
  Status MessageUtils::SendMessage( const URL               &url,
                                    Message                 *msg,
                                    ResponseHandler         *handler,
                                    const MessageSendParams &sendParams )
  {
    //--------------------------------------------------------------------------
    // Get the stuff needed to send the message
    //--------------------------------------------------------------------------
    Log        *log        = DefaultEnv::GetLog();
    PostMaster *postMaster = DefaultEnv::GetPostMaster();
    Status      st;

    if( !postMaster )
      return Status( stError, errUninitialized );

    log->Dump( XRootDMsg, "[%s] Sending message %s",
               url.GetHostId().c_str(), msg->GetDescription().c_str() );


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

    ClientRequestHdr *req = (ClientRequestHdr*)msg->GetBuffer();


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
    // Create and set up the message handler
    //--------------------------------------------------------------------------
    XRootDMsgHandler *msgHandler;
    msgHandler = new XRootDMsgHandler( msg, handler, &url, sidMgr );
    msgHandler->SetExpiration( sendParams.expires );
    msgHandler->SetRedirectAsAnswer( !sendParams.followRedirects );
    msgHandler->SetUserBuffer( sendParams.userBuffer,
                               sendParams.userBufferSize );
    if( sendParams.loadBalancer.url.IsValid() )
      msgHandler->SetLoadBalancer( sendParams.loadBalancer );

    HostList *list = 0;
    if( sendParams.hostList )
      msgHandler->SetHostList( sendParams.hostList );
    else
    {
      list = new HostList();
      list->push_back( url );
      msgHandler->SetHostList( list );
    }

    //--------------------------------------------------------------------------
    // Send the messafe
    //--------------------------------------------------------------------------
    st = postMaster->Send( url, msg, msgHandler, sendParams.stateful,
                           sendParams.expires );
    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Unable to send the message %s: %s",
                  url.GetHostId().c_str(), msg->GetDescription().c_str(),
                  st.ToString().c_str() );
      delete msgHandler;
      delete list;
      return st;
    }
    return Status();
  }

  //----------------------------------------------------------------------------
  // Process sending params
  //----------------------------------------------------------------------------
  void MessageUtils::ProcessSendParams( MessageSendParams &sendParams )
  {
    if( sendParams.timeout == 0 )
    {
      Env *env = DefaultEnv::GetEnv();
      int requestTimeout = DefaultRequestTimeout;
      env->GetInt( "RequestTimeout", requestTimeout );
      sendParams.timeout = requestTimeout;
    }

    if( sendParams.expires == 0 )
      sendParams.expires = ::time(0)+sendParams.timeout;
  }
}
