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
    msgHandler->SetChunkList( sendParams.chunkList );
    msgHandler->SetRedirectCounter( sendParams.redirectLimit );

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
    // Send the message
    //--------------------------------------------------------------------------
    st = postMaster->Send( url, msg, msgHandler, sendParams.stateful,
                           sendParams.expires );
    if( !st.IsOK() )
    {
      XRootDTransport::UnMarshallRequest( msg );
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
    //--------------------------------------------------------------------------
    // Timeout
    //--------------------------------------------------------------------------
    Env *env = DefaultEnv::GetEnv();
    if( sendParams.timeout == 0 )
    {
      int requestTimeout = DefaultRequestTimeout;
      env->GetInt( "RequestTimeout", requestTimeout );
      sendParams.timeout = requestTimeout;
    }

    if( sendParams.expires == 0 )
      sendParams.expires = ::time(0)+sendParams.timeout;

    //--------------------------------------------------------------------------
    // Redirect limit
    //--------------------------------------------------------------------------
    if( sendParams.redirectLimit == 0 )
    {
      int redirectLimit = DefaultRedirectLimit;
      env->GetInt( "RedirectLimit", redirectLimit );
      sendParams.redirectLimit = redirectLimit;
    }
  }

  //----------------------------------------------------------------------------
  //! Append cgi to the one already present in the message
  //----------------------------------------------------------------------------
  void MessageUtils::RewriteCGIAndPath( Message              *msg,
                                        const URL::ParamsMap &newCgi,
                                        bool                  replace,
                                        const std::string    &newPath )
  {
    ClientRequest  *req = (ClientRequest *)msg->GetBuffer();
    switch( req->header.requestid )
    {
      case kXR_chmod:
      case kXR_mkdir:
      case kXR_mv:
      case kXR_open:
      case kXR_rm:
      case kXR_rmdir:
      case kXR_stat:
      case kXR_truncate:
      {
        //----------------------------------------------------------------------
        // Get the pointer to the appropriate path
        //----------------------------------------------------------------------
        char *path = msg->GetBuffer( 24 );
        size_t length = req->header.dlen;
        if( req->header.requestid == kXR_mv )
        {
          for( int i = 0; i < req->header.dlen; ++i, ++path, --length )
            if( *path == ' ' )
              break;
          ++path;
          --length;
        }

        //----------------------------------------------------------------------
        // Create a fake URL from an existing CGI
        //----------------------------------------------------------------------
        char *pathWithNull = new char[length+1];
        memcpy( pathWithNull, path, length );
        pathWithNull[length] = 0;
        std::ostringstream o;
        o << "fake://fake:111/" << pathWithNull;
        delete [] pathWithNull;

        URL currentPath( o.str() );
        URL::ParamsMap currentCgi = currentPath.GetParams();
        MergeCGI( currentCgi, newCgi, replace );
        currentPath.SetParams( currentCgi );
        if( !newPath.empty() )
          currentPath.SetPath( newPath );
        std::string newPathWitParams = currentPath.GetPathWithParams();

        //----------------------------------------------------------------------
        // Write the path with the new cgi appended to the message
        //----------------------------------------------------------------------
        uint32_t newDlen = req->header.dlen - length + newPathWitParams.size();
        msg->ReAllocate( 24+newDlen );
        req  = (ClientRequest *)msg->GetBuffer();
        path = msg->GetBuffer( 24 );
        if( req->header.requestid == kXR_mv )
        {
          for( int i = 0; i < req->header.dlen; ++i, ++path )
            if( *path == ' ' )
              break;
          ++path;
        }
        memcpy( path, newPathWitParams.c_str(), newPathWitParams.size() );
        req->header.dlen = newDlen;
        break;
      }
    }
    XRootDTransport::SetDescription( msg );
  }

  //------------------------------------------------------------------------
  //! Merge cgi2 into cgi1
  //------------------------------------------------------------------------
  void MessageUtils::MergeCGI( URL::ParamsMap       &cgi1,
                               const URL::ParamsMap &cgi2,
                               bool                  replace )
  {
    URL::ParamsMap::const_iterator it;
    for( it = cgi2.begin(); it != cgi2.end(); ++it )
    {
      if( replace || cgi1.find( it->first ) == cgi1.end() )
        cgi1[it->first] = it->second;
      else
      {
        std::string &v = cgi1[it->first];
        if( v.empty() )
          v = it->second;
        else
        {
          v += ',';
          v += it->second;
        }
      }
    }
  }
}
