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

#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClLocalFileHandler.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClTls.hh"

#include "XrdOuc/XrdOucCRC.hh"

#include "XrdSys/XrdSysPlatform.hh" // same as above
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPthread.hh"
#include <memory>
#include <sstream>
#include <numeric>

namespace
{
  //----------------------------------------------------------------------------
  // We need an extra task what will run the handler in the future, because
  // tasks get deleted and we need the handler
  //----------------------------------------------------------------------------
  class WaitTask: public XrdCl::Task
  {
    public:
      WaitTask( XrdCl::XRootDMsgHandler *handler ): pHandler( handler )
      {
        std::ostringstream o;
        o << "WaitTask for: 0x" << handler->GetRequest();
        SetName( o.str() );
      }

      virtual time_t Run( time_t now )
      {
        pHandler->WaitDone( now );
        return 0;
      }
    private:
      XrdCl::XRootDMsgHandler *pHandler;
  };
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Delegate the response handling to the thread-pool
  //----------------------------------------------------------------------------
  class HandleRspJob: public XrdCl::Job
  {
    public:
      HandleRspJob( XrdCl::XRootDMsgHandler *handler ): pHandler( handler )
      {

      }

      virtual ~HandleRspJob()
      {

      }

      virtual void Run( void *arg )
      {
        pHandler->HandleResponse();
        delete this;
      }
    private:
      XrdCl::XRootDMsgHandler *pHandler;
  };

  //----------------------------------------------------------------------------
  // Examine an incoming message, and decide on the action to be taken
  //----------------------------------------------------------------------------
  uint16_t XRootDMsgHandler::Examine( std::shared_ptr<Message> &msg )
  {
    //--------------------------------------------------------------------------
    // if the MsgHandler is already being used to process another request
    // (kXR_oksofar) we need to wait
    //--------------------------------------------------------------------------
    if( pOksofarAsAnswer )
    {
      XrdSysCondVarHelper lck( pCV );
      while( pResponse ) pCV.Wait();
    }
    else
    {
      if( pResponse )
      {
        Log *log = DefaultEnv::GetLog();
        log->Warning( ExDbgMsg, "[%s] MsgHandler is examining a response although "
                                "it already owns a response: 0x%x (message: %s ).",
                      pUrl.GetHostId().c_str(), this,
                      pRequest->GetDescription().c_str() );
      }
    }

    if( msg->GetSize() < 8 )
      return Ignore;

    ServerResponse *rsp    = (ServerResponse *)msg->GetBuffer();
    ClientRequest  *req    = (ClientRequest *)pRequest->GetBuffer();
    uint16_t        status = 0;
    uint32_t        dlen   = 0;

    //--------------------------------------------------------------------------
    // We only care about async responses, but those are extracted now
    // in the SocketHandler.
    //--------------------------------------------------------------------------
    if( rsp->hdr.status == kXR_attn )
    {
      return Ignore;
    }
    //--------------------------------------------------------------------------
    // We got a sync message - check if it belongs to us
    //--------------------------------------------------------------------------
    else
    {
      if( rsp->hdr.streamid[0] != req->header.streamid[0] ||
          rsp->hdr.streamid[1] != req->header.streamid[1] )
        return Ignore;

      status = rsp->hdr.status;
      dlen   = rsp->hdr.dlen;
    }

    //--------------------------------------------------------------------------
    // We take the ownership of the message and decide what we will do
    // with the handler itself, the options are:
    // 1) we want to either read in raw mode (the Raw flag) or have the message
    //    body reconstructed for us by the TransportHandler by the time
    //    Process() is called (default, no extra flag)
    // 2) we either got a full response in which case we don't want to be
    //    notified about anything anymore (RemoveHandler) or we got a partial
    //    answer and we need to wait for more (default, no extra flag)
    //--------------------------------------------------------------------------
    pResponse = msg;
    pBodyReader->SetDataLength( dlen );

    Log *log = DefaultEnv::GetLog();
    switch( status )
    {
      //------------------------------------------------------------------------
      // Handle the cached cases
      //------------------------------------------------------------------------
      case kXR_error:
      case kXR_redirect:
      case kXR_wait:
        return RemoveHandler;

      case kXR_waitresp:
      {
        log->Dump( XRootDMsg, "[%s] Got kXR_waitresp response to "
                   "message %s", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        pResponse.reset();
        return Ignore; // This must be handled synchronously!
      }

      //------------------------------------------------------------------------
      // Handle the potential raw cases
      //------------------------------------------------------------------------
      case kXR_ok:
      {
        //----------------------------------------------------------------------
        // For kXR_read we read in raw mode
        //----------------------------------------------------------------------
        uint16_t reqId = ntohs( req->header.requestid );
        if( reqId == kXR_read )
        {
          return Raw | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // kXR_readv is the same as kXR_read
        //----------------------------------------------------------------------
        if( reqId == kXR_readv )
        {
          return Raw | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // For everything else we just take what we got
        //----------------------------------------------------------------------
        return RemoveHandler;
      }

      //------------------------------------------------------------------------
      // kXR_oksofars are special, they are not full responses, so we reset
      // the response pointer to 0 and add the message to the partial list
      //------------------------------------------------------------------------
      case kXR_oksofar:
      {
        log->Dump( XRootDMsg, "[%s] Got a kXR_oksofar response to request "
                   "%s", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        if( !pOksofarAsAnswer )
        {
          pPartialResps.emplace_back( std::move( pResponse ) );
        }

        //----------------------------------------------------------------------
        // For kXR_read we either read in raw mode if the message has not
        // been fully reconstructed already, if it has, we adjust
        // the buffer offset to prepare for the next one
        //----------------------------------------------------------------------
        uint16_t reqId = ntohs( req->header.requestid );
        if( reqId == kXR_read )
        {
          pTimeoutFence.store( true, std::memory_order_relaxed );
          return Raw | ( pOksofarAsAnswer ? None : NoProcess );
        }

        //----------------------------------------------------------------------
        // kXR_readv is similar to read, except that the payload is different
        //----------------------------------------------------------------------
        if( reqId == kXR_readv )
        {
          pTimeoutFence.store( true, std::memory_order_relaxed );
          return Raw | ( pOksofarAsAnswer ? None : NoProcess );
        }

        return ( pOksofarAsAnswer ? None : NoProcess );
      }

      case kXR_status:
      {
        log->Dump( XRootDMsg, "[%s] Got a kXR_status response to request "
                   "%s", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        uint16_t reqId = ntohs( req->header.requestid );
        if( reqId == kXR_pgwrite )
        {
          //--------------------------------------------------------------------
          // In case of pgwrite by definition this wont be a partial response
          // so we can already remove the handler from the in-queue
          //--------------------------------------------------------------------
          return RemoveHandler;
        }

        //----------------------------------------------------------------------
        // Otherwise (pgread), first of all we need to read the body of the
        // kXR_status response, we can handle the raw data (if any) only after
        // we have the whole kXR_status body
        //----------------------------------------------------------------------
        pTimeoutFence.store( true, std::memory_order_relaxed );
        return None;
      }

      //------------------------------------------------------------------------
      // Default
      //------------------------------------------------------------------------
      default:
        return RemoveHandler;
    }
    return RemoveHandler;
  }

  //----------------------------------------------------------------------------
  // Reexamine the incoming message, and decide on the action to be taken
  //----------------------------------------------------------------------------
  uint16_t XRootDMsgHandler::InspectStatusRsp()
  {
    if( !pResponse )
      return 0;

    Log *log = DefaultEnv::GetLog();
    ServerResponse *rsp = (ServerResponse *)pResponse->GetBuffer();

    //--------------------------------------------------------------------------
    // Additional action is only required for kXR_status
    //--------------------------------------------------------------------------
    if( rsp->hdr.status != kXR_status ) return 0;

    //--------------------------------------------------------------------------
    // Ignore malformed status response
    //--------------------------------------------------------------------------
    if( pResponse->GetSize() < sizeof( ServerResponseStatus ) )
    {
      log->Error( XRootDMsg, "[%s] kXR_status: invalid message size.", pUrl.GetHostId().c_str() );
      return Corrupted;
    }

    ClientRequest  *req    = (ClientRequest *)pRequest->GetBuffer();
    uint16_t reqId = ntohs( req->header.requestid );
    //--------------------------------------------------------------------------
    // Unmarshal the status body
    //--------------------------------------------------------------------------
    XRootDStatus st = XRootDTransport::UnMarshalStatusBody( *pResponse, reqId );

    if( !st.IsOK() && st.code == errDataError )
    {
      log->Error( XRootDMsg, "[%s] %s", pUrl.GetHostId().c_str(),
                  st.GetErrorMessage().c_str() );
      return Corrupted;
    }

    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Failed to unmarshall status body.",
                  pUrl.GetHostId().c_str() );
      pStatus = st;
      HandleRspOrQueue();
      return Ignore;
    }

    //--------------------------------------------------------------------------
    // Common handling for partial results
    //--------------------------------------------------------------------------
    ServerResponseV2 *rspst   = (ServerResponseV2*)pResponse->GetBuffer();
    if( rspst->status.bdy.resptype == XrdProto::kXR_PartialResult )
    {
      pPartialResps.push_back( std::move( pResponse ) );
    }

    //--------------------------------------------------------------------------
    // Decide the actions that we need to take
    //--------------------------------------------------------------------------
    uint16_t action = 0;
    if( reqId == kXR_pgread )
    {
      //----------------------------------------------------------------------
      // The message contains only Status header and body but no raw data
      //----------------------------------------------------------------------
      if( !pPageReader )
        pPageReader.reset( new AsyncPageReader( *pChunkList, pCrc32cDigests ) );
      pPageReader->SetRsp( rspst );

      action |= Raw;

      if( rspst->status.bdy.resptype == XrdProto::kXR_PartialResult )
        action |= NoProcess;
      else
        action |= RemoveHandler;
    }
    else if( reqId == kXR_pgwrite )
    {
      // if data corruption has been detected on the server side we will
      // send some additional data pointing to the pages that need to be
      // retransmitted
      if( size_t( sizeof( ServerResponseHeader ) + rspst->status.hdr.dlen + rspst->status.bdy.dlen ) >
        pResponse->GetCursor() )
        action |= More;
    }

    return action;
  }

  //----------------------------------------------------------------------------
  // Get handler sid
  //----------------------------------------------------------------------------
  uint16_t XRootDMsgHandler::GetSid() const
  {
    ClientRequest* req = (ClientRequest*) pRequest->GetBuffer();
    return ((uint16_t)req->header.streamid[1] << 8) | (uint16_t)req->header.streamid[0];
  }

  //----------------------------------------------------------------------------
  //! Process the message if it was "taken" by the examine action
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::Process()
  {
    Log *log = DefaultEnv::GetLog();

    ServerResponse *rsp = (ServerResponse *)pResponse->GetBuffer();

    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();

    //--------------------------------------------------------------------------
    // If it is a local file, it can be only a metalink redirector
    //--------------------------------------------------------------------------
    if( pUrl.IsLocalFile() && pUrl.IsMetalink() )
      pHosts->back().protocol = kXR_PROTOCOLVERSION;

    //--------------------------------------------------------------------------
    // We got an answer, check who we were talking to
    //--------------------------------------------------------------------------
    else
    {
      AnyObject  qryResult;
      int       *qryResponse = 0;
      pPostMaster->QueryTransport( pUrl, XRootDQuery::ServerFlags, qryResult );
      qryResult.Get( qryResponse );
      pHosts->back().flags = *qryResponse; delete qryResponse; qryResponse = 0;
      pPostMaster->QueryTransport( pUrl, XRootDQuery::ProtocolVersion, qryResult );
      qryResult.Get( qryResponse );
      pHosts->back().protocol = *qryResponse; delete qryResponse;
    }

    //--------------------------------------------------------------------------
    // Process the message
    //--------------------------------------------------------------------------
    Status st = XRootDTransport::UnMarshallBody( pResponse.get(), req->header.requestid );
    if( !st.IsOK() )
    {
      pStatus = Status( stFatal, errInvalidMessage );
      HandleResponse();
      return;
    }

    //--------------------------------------------------------------------------
    // we have an response for the message so it's not in fly anymore
    //--------------------------------------------------------------------------
    pMsgInFly = false;

    //--------------------------------------------------------------------------
    // Reset the aggregated wait (used to omit wait response in case of Metalink
    // redirector)
    //--------------------------------------------------------------------------
    if( rsp->hdr.status != kXR_wait )
      pAggregatedWaitTime = 0;

    switch( rsp->hdr.status )
    {
      //------------------------------------------------------------------------
      // kXR_ok - we're done here
      //------------------------------------------------------------------------
      case kXR_ok:
      {
        log->Dump( XRootDMsg, "[%s] Got a kXR_ok response to request %s",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );
        pStatus  = Status();
        HandleResponse();
        return;
      }

      case kXR_status:
      {
        log->Dump( XRootDMsg, "[%s] Got a kXR_status response to request %s",
                    pUrl.GetHostId().c_str(),
                    pRequest->GetDescription().c_str() );
        pStatus   = Status();
        HandleResponse();
        return;
      }

      //------------------------------------------------------------------------
      // kXR_ok - we're serving partial result to the user
      //------------------------------------------------------------------------
      case kXR_oksofar:
      {
        log->Dump( XRootDMsg, "[%s] Got a kXR_oksofar response to request %s",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );
        pStatus  = Status( stOK, suContinue );
        HandleResponse();
        return;
      }

      //------------------------------------------------------------------------
      // kXR_error - we've got a problem
      //------------------------------------------------------------------------
      case kXR_error:
      {
        char *errmsg = new char[rsp->hdr.dlen-3]; errmsg[rsp->hdr.dlen-4] = 0;
        memcpy( errmsg, rsp->body.error.errmsg, rsp->hdr.dlen-4 );
        log->Dump( XRootDMsg, "[%s] Got a kXR_error response to request %s "
                   "[%d] %s", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str(), rsp->body.error.errnum,
                   errmsg );
        delete [] errmsg;

        HandleError( Status(stError, errErrorResponse, rsp->body.error.errnum) );
        return;
      }

      //------------------------------------------------------------------------
      // kXR_redirect - they tell us to go elsewhere
      //------------------------------------------------------------------------
      case kXR_redirect:
      {
        if( rsp->hdr.dlen <= 4 )
        {
          log->Error( XRootDMsg, "[%s] Got invalid redirect response.",
                      pUrl.GetHostId().c_str() );
          pStatus = Status( stError, errInvalidResponse );
          HandleResponse();
          return;
        }

        char *urlInfoBuff = new char[rsp->hdr.dlen-3];
        urlInfoBuff[rsp->hdr.dlen-4] = 0;
        memcpy( urlInfoBuff, rsp->body.redirect.host, rsp->hdr.dlen-4 );
        std::string urlInfo = urlInfoBuff;
        delete [] urlInfoBuff;
        log->Dump( XRootDMsg, "[%s] Got kXR_redirect response to "
                   "message %s: %s, port %d", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str(), urlInfo.c_str(),
                   rsp->body.redirect.port );

        //----------------------------------------------------------------------
        // Check if we can proceed
        //----------------------------------------------------------------------
        if( !pRedirectCounter )
        {
          log->Warning( XRootDMsg, "[%s] Redirect limit has been reached for "
                     "message %s, the last known error is: %s",
                     pUrl.GetHostId().c_str(),
                     pRequest->GetDescription().c_str(),
                     pLastError.ToString().c_str() );


          pStatus = Status( stFatal, errRedirectLimit );
          HandleResponse();
          return;
        }
        --pRedirectCounter;

        //----------------------------------------------------------------------
        // Keep the info about this server if we still need to find a load
        // balancer
        //----------------------------------------------------------------------
        uint32_t flags = pHosts->back().flags;
        if( !pHasLoadBalancer )
        {
          if( flags & kXR_isManager )
          {
            //------------------------------------------------------------------
            // If the current server is a meta manager then it supersedes
            // any existing load balancer, otherwise we assign a load-balancer
            // only if it has not been already assigned
            //------------------------------------------------------------------
            if( ( flags & kXR_attrMeta ) || !pLoadBalancer.url.IsValid() )
            {
              pLoadBalancer = pHosts->back();
              log->Dump( XRootDMsg, "[%s] Current server has been assigned "
                         "as a load-balancer for message %s",
                         pUrl.GetHostId().c_str(),
                         pRequest->GetDescription().c_str() );
              HostList::iterator it;
              for( it = pHosts->begin(); it != pHosts->end(); ++it )
                it->loadBalancer = false;
              pHosts->back().loadBalancer = true;
            }
          }
        }

        //----------------------------------------------------------------------
        // If the redirect comes from a data server safe the URL because
        // in case of a failure we will use it as the effective data server URL
        // for the tried CGI opaque info
        //----------------------------------------------------------------------
        if( flags & kXR_isServer )
          pEffectiveDataServerUrl = new URL( pHosts->back().url );

        //----------------------------------------------------------------------
        // Build the URL and check it's validity
        //----------------------------------------------------------------------
        std::vector<std::string> urlComponents;
        std::string newCgi;
        Utils::splitString( urlComponents, urlInfo, "?" );

        std::ostringstream o;

        o << urlComponents[0];
        if( rsp->body.redirect.port > 0 )
          o << ":" << rsp->body.redirect.port << "/";
        else if( rsp->body.redirect.port < 0 )
        {
          //--------------------------------------------------------------------
          // check if the manager wants to enforce write recovery at himself
          // (beware we are dealing here with negative flags)
          //--------------------------------------------------------------------
          if( ~uint32_t( rsp->body.redirect.port ) & kXR_recoverWrts )
            pHosts->back().flags |= kXR_recoverWrts;

          //--------------------------------------------------------------------
          // check if the manager wants to collapse the communication channel
          // (the redirect host is to replace the current host)
          //--------------------------------------------------------------------
          if( ~uint32_t( rsp->body.redirect.port ) & kXR_collapseRedir )
          {
            std::string url( rsp->body.redirect.host, rsp->hdr.dlen-4 );
            pPostMaster->CollapseRedirect( pUrl, url );
          }

          if( ~uint32_t( rsp->body.redirect.port ) & kXR_ecRedir )
          {
            std::string url( rsp->body.redirect.host, rsp->hdr.dlen-4 );
            if( Utils::CheckEC( pRequest, url ) )
              pRedirectAsAnswer = true;
          }
        }

        URL newUrl = URL( o.str() );
        if( !newUrl.IsValid() )
        {
          pStatus = Status( stError, errInvalidRedirectURL );
          log->Error( XRootDMsg, "[%s] Got invalid redirection URL: %s",
                      pUrl.GetHostId().c_str(), urlInfo.c_str() );
          HandleResponse();
          return;
        }

        if( pUrl.GetUserName() != "" && newUrl.GetUserName() == "" )
          newUrl.SetUserName( pUrl.GetUserName() );

        if( pUrl.GetPassword() != "" && newUrl.GetPassword() == "" )
          newUrl.SetPassword( pUrl.GetPassword() );

        //----------------------------------------------------------------------
        // Forward any "xrd.*" params from the original client request also to
        // the new redirection url
        // Also, we need to preserve any "xrdcl.*' as they are important for
        // our internal workflows.
        //----------------------------------------------------------------------
        std::ostringstream ossXrd;
        const URL::ParamsMap &urlParams = pUrl.GetParams();

        for(URL::ParamsMap::const_iterator it = urlParams.begin();
            it != urlParams.end(); ++it )
        {
          if( it->first.compare( 0, 4, "xrd." ) &&
              it->first.compare( 0, 6, "xrdcl." ) )
            continue;

          ossXrd << it->first << '=' << it->second << '&';
        }

        std::string xrdCgi = ossXrd.str();
        pRedirectUrl = newUrl.GetURL();

        URL cgiURL;
        if( urlComponents.size() > 1 )
        {
          pRedirectUrl += "?";
          pRedirectUrl += urlComponents[1];
          std::ostringstream o;
          o << "fake://fake:111//fake?";
          o << urlComponents[1];

          if( urlComponents.size() == 3 )
            o << '?' << urlComponents[2];

          if (!xrdCgi.empty())
          {
            o << '&' << xrdCgi;
            pRedirectUrl += '&';
                  pRedirectUrl += xrdCgi;
          }

          cgiURL = URL( o.str() );
        }
        else {
          if (!xrdCgi.empty())
          {
            std::ostringstream o;
            o << "fake://fake:111//fake?";
            o << xrdCgi;
            cgiURL = URL( o.str() );
            pRedirectUrl += '?';
            pRedirectUrl += xrdCgi;
          }
        }

        //----------------------------------------------------------------------
        // Check if we need to return the URL as a response
        //----------------------------------------------------------------------
        if( newUrl.GetProtocol() != "root"  && newUrl.GetProtocol() != "xroot"  &&
            newUrl.GetProtocol() != "roots" && newUrl.GetProtocol() != "xroots" &&
            !newUrl.IsLocalFile() )
          pRedirectAsAnswer = true;

        if( pRedirectAsAnswer )
        {
          pStatus   = Status( stError, errRedirect );
          HandleResponse();
          return;
        }

        //----------------------------------------------------------------------
        // Rewrite the message in a way required to send it to another server
        //----------------------------------------------------------------------
        newUrl.SetParams( cgiURL.GetParams() );
        Status st = RewriteRequestRedirect( newUrl );
        if( !st.IsOK() )
        {
          pStatus = st;
          HandleResponse();
          return;
        }

        //----------------------------------------------------------------------
        // Make sure we don't change the protocol by accident (root vs roots)
        //----------------------------------------------------------------------
        if( ( pUrl.GetProtocol() == "roots" || pUrl.GetProtocol() == "xroots" ) &&
            ( newUrl.GetProtocol() == "root" || newUrl.GetProtocol() == "xroot" ) )
          newUrl.SetProtocol( "roots" );

        //----------------------------------------------------------------------
        // Send the request to the new location
        //----------------------------------------------------------------------
        HandleError( RetryAtServer( newUrl, RedirectEntry::EntryRedirect ) );
        return;
      }

      //------------------------------------------------------------------------
      // kXR_wait - we wait, and re-issue the request later
      //------------------------------------------------------------------------
      case kXR_wait:
      {
        uint32_t waitSeconds = 0;

        if( rsp->hdr.dlen >= 4 )
        {
          char *infoMsg = new char[rsp->hdr.dlen-3];
          infoMsg[rsp->hdr.dlen-4] = 0;
          memcpy( infoMsg, rsp->body.wait.infomsg, rsp->hdr.dlen-4 );
          log->Dump( XRootDMsg, "[%s] Got kXR_wait response of %d seconds to "
                     "message %s: %s", pUrl.GetHostId().c_str(),
                     rsp->body.wait.seconds, pRequest->GetDescription().c_str(),
                     infoMsg );
          delete [] infoMsg;
          waitSeconds = rsp->body.wait.seconds;
        }
        else
        {
          log->Dump( XRootDMsg, "[%s] Got kXR_wait response of 0 seconds to "
                     "message %s", pUrl.GetHostId().c_str(),
                     pRequest->GetDescription().c_str() );
        }

        pAggregatedWaitTime += waitSeconds;

        // We need a special case if the data node comes from metalink
        // redirector. In this case it might make more sense to try the
        // next entry in the Metalink than wait.
        if( OmitWait( *pRequest, pLoadBalancer.url ) )
        {
          int maxWait = DefaultMaxMetalinkWait;
          DefaultEnv::GetEnv()->GetInt( "MaxMetalinkWait", maxWait );
          if( pAggregatedWaitTime > maxWait )
          {
            UpdateTriedCGI();
            HandleError( RetryAtServer( pLoadBalancer.url, RedirectEntry::EntryRedirectOnWait ) );
            return;
          }
        }

        //----------------------------------------------------------------------
        // Some messages require rewriting before they can be sent again
        // after wait
        //----------------------------------------------------------------------
        Status st = RewriteRequestWait();
        if( !st.IsOK() )
        {
          pStatus = st;
          HandleResponse();
          return;
        }

        //----------------------------------------------------------------------
        // Register a task to resend the message in some seconds, if we still
        // have time to do that, and report a timeout otherwise
        //----------------------------------------------------------------------
        time_t resendTime = ::time(0)+waitSeconds;

        if( resendTime < pExpiration )
        {
          log->Debug( ExDbgMsg, "[%s] Scheduling WaitTask for MsgHandler: 0x%x (message: %s ).",
                      pUrl.GetHostId().c_str(), this,
                      pRequest->GetDescription().c_str() );

          TaskManager *taskMgr = pPostMaster->GetTaskManager();
          taskMgr->RegisterTask( new WaitTask( this ), resendTime );
        }
        else
        {
          log->Debug( XRootDMsg, "[%s] Wait time is too long, timing out %s",
                      pUrl.GetHostId().c_str(),
                      pRequest->GetDescription().c_str() );
          HandleError( Status( stError, errOperationExpired) );
        }
        return;
      }

      //------------------------------------------------------------------------
      // kXR_waitresp - the response will be returned in some seconds as an
      // unsolicited message. Currently all messages of this type are handled
      // one step before in the XrdClStream::OnIncoming as they need to be
      // processed synchronously.
      //------------------------------------------------------------------------
      case kXR_waitresp:
      {
        if( rsp->hdr.dlen < 4 )
        {
          log->Error( XRootDMsg, "[%s] Got invalid waitresp response.",
                      pUrl.GetHostId().c_str() );
          pStatus = Status( stError, errInvalidResponse );
          HandleResponse();
          return;
        }

        log->Dump( XRootDMsg, "[%s] Got kXR_waitresp response of %d seconds to "
                   "message %s", pUrl.GetHostId().c_str(),
                   rsp->body.waitresp.seconds,
                   pRequest->GetDescription().c_str() );
        return;
      }

      //------------------------------------------------------------------------
      // Default - unrecognized/unsupported response, declare an error
      //------------------------------------------------------------------------
      default:
      {
        log->Dump( XRootDMsg, "[%s] Got unrecognized response %d to "
                   "message %s", pUrl.GetHostId().c_str(),
                   rsp->hdr.status, pRequest->GetDescription().c_str() );
        pStatus   = Status( stError, errInvalidResponse );
        HandleResponse();
        return;
      }
    }

    return;
  }

  //----------------------------------------------------------------------------
  // Handle an event other that a message arrival - may be timeout
  //----------------------------------------------------------------------------
  uint8_t XRootDMsgHandler::OnStreamEvent( StreamEvent   event,
                                           XRootDStatus  status )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( XRootDMsg, "[%s] Stream event reported for msg %s",
               pUrl.GetHostId().c_str(), pRequest->GetDescription().c_str() );

    if( event == Ready )
      return 0;

    if( pTimeoutFence.load( std::memory_order_relaxed ) )
      return 0;

    HandleError( status );
    return RemoveHandler;
  }

  //----------------------------------------------------------------------------
  // Read message body directly from a socket
  //----------------------------------------------------------------------------
  XRootDStatus XRootDMsgHandler::ReadMessageBody( Message*,
                                                  Socket   *socket,
                                                  uint32_t &bytesRead )
  {
    ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();
    uint16_t reqId = ntohs( req->header.requestid );

    if( reqId == kXR_pgread )
      return pPageReader->Read( *socket, bytesRead );

    return pBodyReader->Read( *socket, bytesRead );
  }

  //----------------------------------------------------------------------------
  // We're here when we requested sending something over the wire
  // and there has been a status update on this action
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::OnStatusReady( const Message *message,
                                        XRootDStatus   status )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We were successful, so we now need to listen for a response
    //--------------------------------------------------------------------------
    if( status.IsOK() )
    {
      log->Dump( XRootDMsg, "[%s] Message %s has been successfully sent.",
                 pUrl.GetHostId().c_str(), message->GetDescription().c_str() );

      log->Debug( ExDbgMsg, "[%s] Moving MsgHandler: 0x%x (message: %s ) from out-queue to in-queue.",
                  pUrl.GetHostId().c_str(), this,
                  pRequest->GetDescription().c_str() );

      pMsgInFly = true;
      return;
    }

    //--------------------------------------------------------------------------
    // We have failed, recover if possible
    //--------------------------------------------------------------------------
    log->Error( XRootDMsg, "[%s] Impossible to send message %s. Trying to "
                "recover.", pUrl.GetHostId().c_str(),
                message->GetDescription().c_str() );
    HandleError( status );
  }

  //----------------------------------------------------------------------------
  // Are we a raw writer or not?
  //----------------------------------------------------------------------------
  bool XRootDMsgHandler::IsRaw() const
  {
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();
    uint16_t reqId = ntohs( req->header.requestid );
    if( reqId == kXR_write || reqId == kXR_writev || reqId == kXR_pgwrite )
      return true;
    // checkpoint + execute
    if( reqId == kXR_chkpoint && req->chkpoint.opcode == kXR_ckpXeq )
    {
      ClientRequest *xeq = (ClientRequest*)pRequest->GetBuffer( sizeof( ClientRequest ) );
      reqId = ntohs( xeq->header.requestid );
      return reqId != kXR_truncate; // only checkpointed truncate does not have raw data
    }

    return false;
  }

  //----------------------------------------------------------------------------
  // Write the message body
  //----------------------------------------------------------------------------
  XRootDStatus XRootDMsgHandler::WriteMessageBody( Socket   *socket,
                                                   uint32_t &bytesWritten )
  {
    //--------------------------------------------------------------------------
    // First check if it is a PgWrite
    //--------------------------------------------------------------------------
    if( !pChunkList->empty() && !pCrc32cDigests.empty() )
    {
      //------------------------------------------------------------------------
      // PgWrite will have just one chunk
      //------------------------------------------------------------------------
      ChunkInfo chunk = pChunkList->front();
      //------------------------------------------------------------------------
      // Calculate the size of the first and last page (in case the chunk is not
      // 4KB aligned)
      //------------------------------------------------------------------------
      int fLen = 0, lLen = 0;
      size_t nbpgs = XrdOucPgrwUtils::csNum( chunk.offset, chunk.length, fLen, lLen );

      //------------------------------------------------------------------------
      // Set the crc32c buffer if not ready yet
      //------------------------------------------------------------------------
      if( pPgWrtCksumBuff.GetCursor() == 0 )
      {
        uint32_t digest = htonl( pCrc32cDigests[pPgWrtCurrentPageNb] );
        memcpy( pPgWrtCksumBuff.GetBuffer(), &digest, sizeof( uint32_t ) );
      }

      uint32_t btsLeft = chunk.length - pAsyncOffset;
      uint32_t pglen   = ( pPgWrtCurrentPageNb == 0 ? fLen : XrdSys::PageSize ) - pPgWrtCurrentPageOffset;
      if( pglen > btsLeft ) pglen = btsLeft;
      char*    pgbuf   = static_cast<char*>( chunk.buffer ) + pAsyncOffset;

      while( btsLeft > 0 )
      {
        // first write the crc32c digest
        while( pPgWrtCksumBuff.GetCursor() < sizeof( uint32_t ) )
        {
          uint32_t dgstlen = sizeof( uint32_t ) - pPgWrtCksumBuff.GetCursor();
          char*    dgstbuf = pPgWrtCksumBuff.GetBufferAtCursor();
          int btswrt = 0;
          Status st = socket->Send( dgstbuf, dgstlen, btswrt );
          if( !st.IsOK() ) return st;
          bytesWritten += btswrt;
          pPgWrtCksumBuff.AdvanceCursor( btswrt );
          if( st.code == suRetry ) return st;
        }
        // then write the raw data (one page)
        int btswrt = 0;
        Status st = socket->Send( pgbuf, pglen, btswrt );
        if( !st.IsOK() ) return st;
        pgbuf        += btswrt;
        pglen        -= btswrt;
        btsLeft      -= btswrt;
        bytesWritten += btswrt;
        pAsyncOffset += btswrt; // update the offset to the raw data
        if( st.code == suRetry ) return st;
        // if we managed to write all the data ...
        if( pglen == 0 )
        {
          // move to the next page
          ++pPgWrtCurrentPageNb;
          if( pPgWrtCurrentPageNb < nbpgs )
          {
            // set the digest buffer
            pPgWrtCksumBuff.SetCursor( 0 );
            uint32_t digest = htonl( pCrc32cDigests[pPgWrtCurrentPageNb] );
            memcpy( pPgWrtCksumBuff.GetBuffer(), &digest, sizeof( uint32_t ) );
          }
          // set the page length
          pglen = XrdSys::PageSize;
          if( pglen > btsLeft ) pglen = btsLeft;
          // reset offset in the current page
          pPgWrtCurrentPageOffset = 0;
        }
        else
          // otherwise just adjust the offset in the current page
          pPgWrtCurrentPageOffset += btswrt;

      }
    }
    else if( !pChunkList->empty() )
    {
      size_t size = pChunkList->size();
      for( size_t i = pAsyncChunkIndex ; i < size; ++i )
      {
        char     *buffer          = (char*)(*pChunkList)[i].buffer;
        uint32_t  size            = (*pChunkList)[i].length;
        size_t    leftToBeWritten = size - pAsyncOffset;

        while( leftToBeWritten )
        {
          int btswrt = 0;
          Status st = socket->Send( buffer + pAsyncOffset, leftToBeWritten, btswrt );
          bytesWritten += btswrt;
          if( !st.IsOK() || st.code == suRetry ) return st;
          pAsyncOffset    += btswrt;
          leftToBeWritten -= btswrt;
        }
        //----------------------------------------------------------------------
        // Remember that we have moved to the next chunk, also clear the offset
        // within the buffer as we are going to move to a new one
        //----------------------------------------------------------------------
        ++pAsyncChunkIndex;
        pAsyncOffset = 0;
      }
    }
    else
    {
      Log *log = DefaultEnv::GetLog();

      //------------------------------------------------------------------------
      // If the socket is encrypted we cannot use a kernel buffer, we have to
      // convert to user space buffer
      //------------------------------------------------------------------------
      if( socket->IsEncrypted() )
      {
        log->Debug( XRootDMsg, "[%s] Channel is encrypted: cannot use kernel buffer.",
                    pUrl.GetHostId().c_str() );

        char *ubuff = 0;
        ssize_t ret = XrdSys::Move( *pKBuff, ubuff );
        if( ret < 0 ) return Status( stError, errInternal );
        pChunkList->push_back( ChunkInfo( 0, ret, ubuff ) );
        return WriteMessageBody( socket, bytesWritten );
      }

      //------------------------------------------------------------------------
      // Send the data
      //------------------------------------------------------------------------
      while( !pKBuff->Empty() )
      {
        int btswrt = 0;
        Status st = socket->Send( *pKBuff, btswrt );
        bytesWritten += btswrt;
        if( !st.IsOK() || st.code == suRetry ) return st;
      }

      log->Debug( XRootDMsg, "[%s] Request %s payload (kernel buffer) transferred to socket.",
                  pUrl.GetHostId().c_str(), pRequest->GetDescription().c_str() );
    }

    return Status();
  }

  //----------------------------------------------------------------------------
  // We're here when we got a time event. We needed to re-issue the request
  // in some time in the future, and that moment has arrived
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::WaitDone( time_t )
  {
    HandleError( RetryAtServer( pUrl, RedirectEntry::EntryWait ) );
  }

  //----------------------------------------------------------------------------
  // Bookkeeping after partial response has been received.
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::PartialReceived()
  {
    pTimeoutFence.store( false, std::memory_order_relaxed ); // Take down the timeout fence
  }

  //----------------------------------------------------------------------------
  // Unpack the message and call the response handler
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::HandleResponse()
  {
    //--------------------------------------------------------------------------
    // Process the response and notify the listener
    //--------------------------------------------------------------------------
    XRootDTransport::UnMarshallRequest( pRequest );
    XRootDStatus *status   = ProcessStatus();
    AnyObject    *response = 0;

    Log *log = DefaultEnv::GetLog();
    log->Debug( ExDbgMsg, "[%s] Calling MsgHandler: 0x%x (message: %s ) "
                "with status: %s.",
                pUrl.GetHostId().c_str(), this,
                pRequest->GetDescription().c_str(),
                status->ToString().c_str() );

    if( status->IsOK() )
    {
      Status st = ParseResponse( response );
      if( !st.IsOK() )
      {
        delete status;
        delete response;
        status   = new XRootDStatus( st );
        response = 0;
      }
    }

    //--------------------------------------------------------------------------
    // Close the redirect entry if necessary
    //--------------------------------------------------------------------------
    if( pRdirEntry )
    {
      pRdirEntry->status = *status;
      pRedirectTraceBack.push_back( std::move( pRdirEntry ) );
    }

    //--------------------------------------------------------------------------
    // Is it a final response?
    //--------------------------------------------------------------------------
    bool finalrsp = !( pStatus.IsOK() && pStatus.code == suContinue );

    //--------------------------------------------------------------------------
    // Release the stream id
    //--------------------------------------------------------------------------
    if( pSidMgr && finalrsp )
    {
      ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();
      if( status->IsOK() || !pMsgInFly ||
          !( status->code == errOperationExpired || status->code == errOperationInterrupted ) )
        pSidMgr->ReleaseSID( req->header.streamid );
    }

    HostList *hosts = pHosts.release();
    if( !finalrsp )
      pHosts.reset( new HostList( *hosts ) );

    pResponseHandler->HandleResponseWithHosts( status, response, hosts );

    //--------------------------------------------------------------------------
    // if it is the final response there is nothing more to do ...
    //--------------------------------------------------------------------------
    if( finalrsp )
      delete this;
    //--------------------------------------------------------------------------
    // on the other hand if it is not the final response, we have to keep the
    // MsgHandler and delete the current response
    //--------------------------------------------------------------------------
    else
    {
      XrdSysCondVarHelper lck( pCV );
      pResponse.reset();
      pTimeoutFence.store( false, std::memory_order_relaxed );
      pCV.Broadcast();
    }
  }


  //----------------------------------------------------------------------------
  // Extract the status information from the stuff that we got
  //----------------------------------------------------------------------------
  XRootDStatus *XRootDMsgHandler::ProcessStatus()
  {
    XRootDStatus   *st  = new XRootDStatus( pStatus );
    ServerResponse *rsp = 0;
    if( pResponse )
      rsp = (ServerResponse *)pResponse->GetBuffer();

    if( !pStatus.IsOK() && rsp )
    {
      if( pStatus.code == errErrorResponse )
      {
        st->errNo = rsp->body.error.errnum;
        // omit the last character as the string returned from the server
        // (acording to protocol specs) should be null-terminated
        std::string errmsg( rsp->body.error.errmsg, rsp->hdr.dlen-5 );
        if( st->errNo == kXR_noReplicas && !pLastError.IsOK() )
          errmsg += " Last seen error: " + pLastError.ToString();
        st->SetErrorMessage( errmsg );
      }
      else if( pStatus.code == errRedirect )
        st->SetErrorMessage( pRedirectUrl );
    }
    return st;
  }

  //------------------------------------------------------------------------
  // Parse the response and put it in an object that could be passed to
  // the user
  //------------------------------------------------------------------------
  Status XRootDMsgHandler::ParseResponse( AnyObject *&response )
  {
    if( !pResponse )
      return Status();

    ServerResponse *rsp = (ServerResponse *)pResponse->GetBuffer();
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();
    Log            *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Handle redirect as an answer
    //--------------------------------------------------------------------------
    if( rsp->hdr.status == kXR_redirect )
    {
      log->Error( XRootDMsg, "Internal Error: unable to process redirect" );
      return 0;
    }

    Buffer    buff;
    uint32_t  length = 0;
    char     *buffer = 0;

    //--------------------------------------------------------------------------
    // We don't have any partial answers so pass what we have
    //--------------------------------------------------------------------------
    if( pPartialResps.empty() )
    {
      buffer = rsp->body.buffer.data;
      length = rsp->hdr.dlen;
    }
    //--------------------------------------------------------------------------
    // Partial answers, we need to glue them together before parsing
    //--------------------------------------------------------------------------
    else if( req->header.requestid != kXR_read &&
             req->header.requestid != kXR_readv )
    {
      for( uint32_t i = 0; i < pPartialResps.size(); ++i )
      {
        ServerResponse *part = (ServerResponse*)pPartialResps[i]->GetBuffer();
        length += part->hdr.dlen;
      }
      length += rsp->hdr.dlen;

      buff.Allocate( length );
      uint32_t offset = 0;
      for( uint32_t i = 0; i < pPartialResps.size(); ++i )
      {
        ServerResponse *part = (ServerResponse*)pPartialResps[i]->GetBuffer();
        buff.Append( part->body.buffer.data, part->hdr.dlen, offset );
        offset += part->hdr.dlen;
      }
      buff.Append( rsp->body.buffer.data, rsp->hdr.dlen, offset );
      buffer = buff.GetBuffer();
    }

    //--------------------------------------------------------------------------
    // Right, but what was the question?
    //--------------------------------------------------------------------------
    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // kXR_mv, kXR_truncate, kXR_rm, kXR_mkdir, kXR_rmdir, kXR_chmod,
      // kXR_ping, kXR_close, kXR_write, kXR_sync
      //------------------------------------------------------------------------
      case kXR_mv:
      case kXR_truncate:
      case kXR_rm:
      case kXR_mkdir:
      case kXR_rmdir:
      case kXR_chmod:
      case kXR_ping:
      case kXR_close:
      case kXR_write:
      case kXR_writev:
      case kXR_sync:
      case kXR_chkpoint:
        return Status();

      //------------------------------------------------------------------------
      // kXR_locate
      //------------------------------------------------------------------------
      case kXR_locate:
      {
        AnyObject *obj = new AnyObject();

        char *nullBuffer = new char[length+1];
        nullBuffer[length] = 0;
        memcpy( nullBuffer, buffer, length );

        log->Dump( XRootDMsg, "[%s] Parsing the response to %s as "
                   "LocateInfo: %s", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str(), nullBuffer );
        LocationInfo *data = new LocationInfo();

        if( data->ParseServerResponse( nullBuffer ) == false )
        {
          delete obj;
          delete data;
          delete [] nullBuffer;
          return Status( stError, errInvalidResponse );
        }
        delete [] nullBuffer;

        obj->Set( data );
        response = obj;
        return Status();
      }

      //------------------------------------------------------------------------
      // kXR_stat
      //------------------------------------------------------------------------
      case kXR_stat:
      {
        AnyObject *obj = new AnyObject();

        //----------------------------------------------------------------------
        // Virtual File System stat (kXR_vfs)
        //----------------------------------------------------------------------
        if( req->stat.options & kXR_vfs )
        {
          StatInfoVFS *data = new StatInfoVFS();

          char *nullBuffer = new char[length+1];
          nullBuffer[length] = 0;
          memcpy( nullBuffer, buffer, length );

          log->Dump( XRootDMsg, "[%s] Parsing the response to %s as "
                     "StatInfoVFS: %s", pUrl.GetHostId().c_str(),
                     pRequest->GetDescription().c_str(), nullBuffer );

          if( data->ParseServerResponse( nullBuffer ) == false )
          {
              delete obj;
              delete data;
              delete [] nullBuffer;
              return Status( stError, errInvalidResponse );
          }
          delete [] nullBuffer;

          obj->Set( data );
        }
        //----------------------------------------------------------------------
        // Normal stat
        //----------------------------------------------------------------------
        else
        {
          StatInfo *data = new StatInfo();

          char *nullBuffer = new char[length+1];
          nullBuffer[length] = 0;
          memcpy( nullBuffer, buffer, length );

          log->Dump( XRootDMsg, "[%s] Parsing the response to %s as StatInfo: "
                     "%s", pUrl.GetHostId().c_str(),
                     pRequest->GetDescription().c_str(), nullBuffer );

          if( data->ParseServerResponse( nullBuffer ) == false )
          {
              delete obj;
              delete data;
              delete [] nullBuffer;
              return Status( stError, errInvalidResponse );
          }
          delete [] nullBuffer;
          obj->Set( data );
        }

        response = obj;
        return Status();
      }

      //------------------------------------------------------------------------
      // kXR_protocol
      //------------------------------------------------------------------------
      case kXR_protocol:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to %s as ProtocolInfo",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        if( rsp->hdr.dlen < 8 )
        {
          log->Error( XRootDMsg, "[%s] Got invalid redirect response.",
                      pUrl.GetHostId().c_str() );
          return Status( stError, errInvalidResponse );
        }

        AnyObject *obj = new AnyObject();
        ProtocolInfo *data = new ProtocolInfo( rsp->body.protocol.pval,
                                               rsp->body.protocol.flags );
        obj->Set( data );
        response = obj;
        return Status();
      }

      //------------------------------------------------------------------------
      // kXR_dirlist
      //------------------------------------------------------------------------
      case kXR_dirlist:
      {
        AnyObject *obj = new AnyObject();
        log->Dump( XRootDMsg, "[%s] Parsing the response to %s as "
                   "DirectoryList", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        char *path = new char[req->dirlist.dlen+1];
        path[req->dirlist.dlen] = 0;
        memcpy( path, pRequest->GetBuffer(24), req->dirlist.dlen );

        DirectoryList *data = new DirectoryList();
        data->SetParentName( path );
        delete [] path;

        char *nullBuffer = new char[length+1];
        nullBuffer[length] = 0;
        memcpy( nullBuffer, buffer, length );

        bool invalidrsp = false;

        if( !pDirListStarted )
        {
          pDirListWithStat = DirectoryList::HasStatInfo( nullBuffer );
          pDirListStarted  = true;

          invalidrsp = !data->ParseServerResponse( pUrl.GetHostId(), nullBuffer );
        }
        else
          invalidrsp = !data->ParseServerResponse( pUrl.GetHostId(), nullBuffer, pDirListWithStat );

        if( invalidrsp )
        {
          delete data;
          delete obj;
          delete [] nullBuffer;
          return Status( stError, errInvalidResponse );
        }

        delete [] nullBuffer;
        obj->Set( data );
        response = obj;
        return Status();
      }

      //------------------------------------------------------------------------
      // kXR_open - if we got the statistics, otherwise return 0
      //------------------------------------------------------------------------
      case kXR_open:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to %s as OpenInfo",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        if( rsp->hdr.dlen < 4 )
        {
          log->Error( XRootDMsg, "[%s] Got invalid open response.",
                      pUrl.GetHostId().c_str() );
          return Status( stError, errInvalidResponse );
        }

        AnyObject *obj      = new AnyObject();
        StatInfo  *statInfo = 0;

        //----------------------------------------------------------------------
        // Handle StatInfo if requested
        //----------------------------------------------------------------------
        if( req->open.options & kXR_retstat )
        {
          log->Dump( XRootDMsg, "[%s] Parsing StatInfo in response to %s",
                     pUrl.GetHostId().c_str(),
                     pRequest->GetDescription().c_str() );

          if( rsp->hdr.dlen >= 12 )
          {
            char *nullBuffer = new char[rsp->hdr.dlen-11];
            nullBuffer[rsp->hdr.dlen-12] = 0;
            memcpy( nullBuffer, buffer+12, rsp->hdr.dlen-12 );

            statInfo = new StatInfo();
            if( statInfo->ParseServerResponse( nullBuffer ) == false )
            {
              delete statInfo;
              statInfo = 0;
            }
            delete [] nullBuffer;
          }

          if( rsp->hdr.dlen < 12 || !statInfo )
          {
            log->Error( XRootDMsg, "[%s] Unable to parse StatInfo in response "
                        "to %s", pUrl.GetHostId().c_str(),
                        pRequest->GetDescription().c_str() );
            delete obj;
            return Status( stError, errInvalidResponse );
          }
        }

        OpenInfo *data = new OpenInfo( (uint8_t*)buffer,
                                       pResponse->GetSessionId(),
                                       statInfo );
        obj->Set( data );
        response = obj;
        return Status();
      }

      //------------------------------------------------------------------------
      // kXR_read
      //------------------------------------------------------------------------
      case kXR_read:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to %s as ChunkInfo",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        for( uint32_t i = 0; i < pPartialResps.size(); ++i )
        {
          //--------------------------------------------------------------------
          // we are expecting to have only the header in the message, the raw
          // data have been readout into the user buffer
          //--------------------------------------------------------------------
          if( pPartialResps[i]->GetSize() > 8 )
            return Status( stOK, errInternal );
        }
        //----------------------------------------------------------------------
        // we are expecting to have only the header in the message, the raw
        // data have been readout into the user buffer
        //----------------------------------------------------------------------
        if( pResponse->GetSize() > 8 )
          return Status( stOK, errInternal );
        //----------------------------------------------------------------------
        // Get the response for the end user
        //----------------------------------------------------------------------
        return pBodyReader->GetResponse( response );
      }

      //------------------------------------------------------------------------
      // kXR_pgread
      //------------------------------------------------------------------------
      case kXR_pgread:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to %s as PageInfo",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        //----------------------------------------------------------------------
        // Glue in the cached responses if necessary
        //----------------------------------------------------------------------
        ChunkInfo  chunk         = pChunkList->front();
        bool       sizeMismatch  = false;
        uint32_t   currentOffset = 0;
        char      *cursor        = (char*)chunk.buffer;
        for( uint32_t i = 0; i < pPartialResps.size(); ++i )
        {
          ServerResponseV2 *part  = (ServerResponseV2*)pPartialResps[i]->GetBuffer();

          //--------------------------------------------------------------------
          // the actual size of the raw data without the crc32c checksums
          //--------------------------------------------------------------------
          size_t datalen = part->status.bdy.dlen - NbPgPerRsp( part->info.pgread.offset,
                           part->status.bdy.dlen ) * CksumSize;

          if( currentOffset + datalen > chunk.length )
          {
            sizeMismatch = true;
            break;
          }

          currentOffset += datalen;
          cursor        += datalen;
        }

        ServerResponseV2 *rspst = (ServerResponseV2*)pResponse->GetBuffer();
        size_t datalen = rspst->status.bdy.dlen - NbPgPerRsp( rspst->info.pgread.offset,
                         rspst->status.bdy.dlen ) * CksumSize;
        if( currentOffset + datalen <= chunk.length )
          currentOffset += datalen;
        else
          sizeMismatch = true;

        //----------------------------------------------------------------------
        // Overflow
        //----------------------------------------------------------------------
        if( pChunkStatus.front().sizeError || sizeMismatch )
        {
          log->Error( XRootDMsg, "[%s] Handling response to %s: user supplied "
                      "buffer is too small for the received data.",
                      pUrl.GetHostId().c_str(),
                      pRequest->GetDescription().c_str() );
          return Status( stError, errInvalidResponse );
        }

        AnyObject *obj   = new AnyObject();
        PageInfo *pgInfo = new PageInfo( chunk.offset, currentOffset, chunk.buffer,
                                         std::move( pCrc32cDigests) );

        obj->Set( pgInfo );
        response = obj;
        return Status();
      }

      //------------------------------------------------------------------------
      // kXR_pgwrite
      //------------------------------------------------------------------------
      case kXR_pgwrite:
      {
        std::vector<std::tuple<uint64_t, uint32_t>> retries;

        ServerResponseV2 *rsp = (ServerResponseV2*)pResponse->GetBuffer();
        if( rsp->status.bdy.dlen > 0 )
        {
          ServerResponseBody_pgWrCSE *cse = (ServerResponseBody_pgWrCSE*)pResponse->GetBuffer( sizeof( ServerResponseV2 ) );
          size_t pgcnt = ( rsp->status.bdy.dlen - 8 ) / sizeof( kXR_int64 );
          retries.reserve( pgcnt );
          kXR_int64 *pgoffs = (kXR_int64*)pResponse->GetBuffer( sizeof( ServerResponseV2 ) +
                                                                sizeof( ServerResponseBody_pgWrCSE ) );

          for( size_t i = 0; i < pgcnt; ++i )
          {
            uint32_t len = XrdSys::PageSize;
            if( i == 0 ) len = cse->dlFirst;
            else if( i == pgcnt - 1 ) len = cse->dlLast;
            retries.push_back( std::make_tuple( pgoffs[i], len ) );
          }
        }

        RetryInfo *info = new RetryInfo( std::move( retries ) );
        AnyObject *obj   = new AnyObject();
        obj->Set( info );
        response = obj;

        return Status();
      }


      //------------------------------------------------------------------------
      // kXR_readv - we need to pass the length of the buffer to the user code
      //------------------------------------------------------------------------
      case kXR_readv:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as "
                   "VectorReadInfo", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        for( uint32_t i = 0; i < pPartialResps.size(); ++i )
        {
          //--------------------------------------------------------------------
          // we are expecting to have only the header in the message, the raw
          // data have been readout into the user buffer
          //--------------------------------------------------------------------
          if( pPartialResps[i]->GetSize() > 8 )
            return Status( stOK, errInternal );
        }
        //----------------------------------------------------------------------
        // we are expecting to have only the header in the message, the raw
        // data have been readout into the user buffer
        //----------------------------------------------------------------------
        if( pResponse->GetSize() > 8 )
          return Status( stOK, errInternal );
        //----------------------------------------------------------------------
        // Get the response for the end user
        //----------------------------------------------------------------------
        return pBodyReader->GetResponse( response );
      }

      //------------------------------------------------------------------------
      // kXR_fattr
      //------------------------------------------------------------------------
      case kXR_fattr:
      {
        int   len  = rsp->hdr.dlen;
        char* data = rsp->body.buffer.data;

        return ParseXAttrResponse( data, len, response );
      }

      //------------------------------------------------------------------------
      // kXR_query
      //------------------------------------------------------------------------
      case kXR_query:
      case kXR_set:
      case kXR_prepare:
      default:
      {
        AnyObject *obj = new AnyObject();
        log->Dump( XRootDMsg, "[%s] Parsing the response to %s as BinaryData",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        BinaryDataInfo *data = new BinaryDataInfo();
        data->Allocate( length );
        data->Append( buffer, length );
        obj->Set( data );
        response = obj;
        return Status();
      }
    };
    return Status( stError, errInvalidMessage );
  }

  //------------------------------------------------------------------------
  // Parse the response to kXR_fattr request and put it in an object that
  // could be passed to the user
  //------------------------------------------------------------------------
  Status XRootDMsgHandler::ParseXAttrResponse( char *data, size_t len,
                                               AnyObject *&response )
  {
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();
//    Log            *log = DefaultEnv::GetLog(); //TODO

    switch( req->fattr.subcode )
    {
      case kXR_fattrDel:
      case kXR_fattrSet:
      {
        Status status;

        kXR_char nerrs = 0;
        if( !( status = ReadFromBuffer( data, len, nerrs ) ).IsOK() )
          return status;

        kXR_char nattr = 0;
        if( !( status = ReadFromBuffer( data, len, nattr ) ).IsOK() )
          return status;

        std::vector<XAttrStatus> resp;
        // read the namevec
        for( kXR_char i = 0; i < nattr; ++i )
        {
          kXR_unt16 rc = 0;
          if( !( status = ReadFromBuffer( data, len, rc ) ).IsOK() )
            return status;
          rc = ntohs( rc );

          // count errors
          if( rc ) --nerrs;

          std::string name;
          if( !( status = ReadFromBuffer( data, len, name ) ).IsOK() )
            return status;

          XRootDStatus st = rc ? XRootDStatus( stError, errErrorResponse, rc ) :
                                 XRootDStatus();
          resp.push_back( XAttrStatus( name, st ) );
        }

        // check if we read all the data and if the error count is OK
        if( len != 0 || nerrs != 0 ) return Status( stError, errDataError );

        // set up the response object
        response = new AnyObject();
        response->Set( new std::vector<XAttrStatus>( std::move( resp ) ) );

        return Status();
      }

      case kXR_fattrGet:
      {
        Status status;

        kXR_char nerrs = 0;
        if( !( status = ReadFromBuffer( data, len, nerrs ) ).IsOK() )
          return status;

        kXR_char nattr = 0;
        if( !( status = ReadFromBuffer( data, len, nattr ) ).IsOK() )
          return status;

        std::vector<XAttr> resp;
        resp.reserve( nattr );

        // read the name vec
        for( kXR_char i = 0; i < nattr; ++i )
        {
          kXR_unt16 rc = 0;
          if( !( status = ReadFromBuffer( data, len, rc ) ).IsOK() )
            return status;
          rc = ntohs( rc );

          // count errors
          if( rc ) --nerrs;

          std::string name;
          if( !( status = ReadFromBuffer( data, len, name ) ).IsOK() )
            return status;

          XRootDStatus st = rc ? XRootDStatus( stError, errErrorResponse, rc ) :
                                 XRootDStatus();
          resp.push_back( XAttr( name, st ) );
        }

        // read the value vec
        for( kXR_char i = 0; i < nattr; ++i )
        {
          kXR_int32 vlen = 0;
          if( !( status = ReadFromBuffer( data, len, vlen ) ).IsOK() )
            return status;
          vlen = ntohl( vlen );

          std::string value;
          if( !( status = ReadFromBuffer( data, len, vlen, value ) ).IsOK() )
            return status;

          resp[i].value.swap( value );
        }

        // check if we read all the data and if the error count is OK
        if( len != 0 || nerrs != 0 ) return Status( stError, errDataError );

        // set up the response object
        response = new AnyObject();
        response->Set( new std::vector<XAttr>( std::move( resp ) ) );

        return Status();
      }

      case kXR_fattrList:
      {
        Status status;
        std::vector<XAttr> resp;

        while( len > 0 )
        {
          std::string name;
          if( !( status = ReadFromBuffer( data, len, name ) ).IsOK() )
            return status;

          kXR_int32 vlen = 0;
          if( !( status = ReadFromBuffer( data, len, vlen ) ).IsOK() )
            return status;
          vlen = ntohl( vlen );

          std::string value;
          if( !( status = ReadFromBuffer( data, len, vlen, value ) ).IsOK() )
            return status;

          resp.push_back( XAttr( name, value ) );
        }

        // set up the response object
        response = new AnyObject();
        response->Set( new std::vector<XAttr>( std::move( resp ) ) );

        return Status();
      }

      default:
        return Status( stError, errDataError );
    }
  }

  //----------------------------------------------------------------------------
  // Perform the changes to the original request needed by the redirect
  // procedure - allocate new streamid, append redirection data and such
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::RewriteRequestRedirect( const URL &newUrl )
  {
    Log *log = DefaultEnv::GetLog();

    Status st;
    // Append any "xrd.*" parameters present in newCgi so that any authentication
    // requirements are properly enforced
    const URL::ParamsMap &newCgi = newUrl.GetParams();
    std::string xrdCgi = "";
    std::ostringstream ossXrd;
    for(URL::ParamsMap::const_iterator it = newCgi.begin(); it != newCgi.end(); ++it )
    {
      if( it->first.compare( 0, 4, "xrd." ) )
        continue;
      ossXrd << it->first << '=' << it->second << '&';
    }

    xrdCgi = ossXrd.str();
    // Redirection URL containing also any original xrd.* opaque parameters
    XrdCl::URL authUrl;

    if (xrdCgi.empty())
    {
      authUrl = newUrl;
    }
    else
    {
      std::string surl = newUrl.GetURL();
      (surl.find('?') == std::string::npos) ? (surl += '?') :
          ((*surl.rbegin() != '&') ? (surl += '&') : (surl += ""));
      surl += xrdCgi;

      if (!authUrl.FromString(surl))
      {
        log->Error( XRootDMsg, "[%s] Failed to build redirection URL from data:"
		    "%s", surl.c_str());
        return Status(stError, errInvalidRedirectURL);
      }
    }

    //--------------------------------------------------------------------------
    // Rewrite particular requests
    //--------------------------------------------------------------------------
    XRootDTransport::UnMarshallRequest( pRequest );
    MessageUtils::RewriteCGIAndPath( pRequest, newCgi, true, newUrl.GetPath() );
    XRootDTransport::MarshallRequest( pRequest );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Some requests need to be rewritten also after getting kXR_wait
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::RewriteRequestWait()
  {
    ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();

    XRootDTransport::UnMarshallRequest( pRequest );

    //------------------------------------------------------------------------
    // For kXR_locate and kXR_open request the kXR_refresh bit needs to be
    // turned off after wait
    //------------------------------------------------------------------------
    switch( req->header.requestid )
    {
      case kXR_locate:
      {
        uint16_t refresh = kXR_refresh;
        req->locate.options &= (~refresh);
        break;
      }

      case kXR_open:
      {
        uint16_t refresh = kXR_refresh;
        req->locate.options &= (~refresh);
        break;
      }
    }

    XRootDTransport::SetDescription( pRequest );
    XRootDTransport::MarshallRequest( pRequest );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Recover error
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::HandleError( XRootDStatus status )
  {
    //--------------------------------------------------------------------------
    // If there was no error then do nothing
    //--------------------------------------------------------------------------
    if( status.IsOK() )
      return;

    if( pSidMgr && pMsgInFly && ( 
        status.code == errOperationExpired ||
        status.code == errOperationInterrupted ) )
    {
      ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();
      pSidMgr->TimeOutSID( req->header.streamid );
    }

    bool noreplicas = ( status.code == errErrorResponse &&
                        status.errNo == kXR_noReplicas );

    if( !noreplicas ) pLastError = status;

    Log *log = DefaultEnv::GetLog();
    log->Debug( XRootDMsg, "[%s] Handling error while processing %s: %s.",
                pUrl.GetHostId().c_str(), pRequest->GetDescription().c_str(),
                status.ToString().c_str() );

    //--------------------------------------------------------------------------
    // Check if it is a fatal TLS error that has been marked as potentially
    // recoverable, if yes check if we can downgrade from fatal to error.
    //--------------------------------------------------------------------------
    if( status.IsFatal() && status.code == errTlsError && status.errNo == EAGAIN )
    {
      if( pSslErrCnt < MaxSslErrRetry )
      {
        status.status &= ~stFatal; // switch off fatal&error bits
        status.status |= stError;  // switch on error bit
      }
      ++pSslErrCnt; // count number of consecutive SSL errors
    }
    else
      pSslErrCnt = 0;

    //--------------------------------------------------------------------------
    // We have got an error message, we can recover it at the load balancer if:
    // 1) we haven't got it from the load balancer
    // 2) we have a load balancer assigned
    // 3) the error is either one of: kXR_FSError, kXR_IOError, kXR_ServerError,
    //    kXR_NotFound
    // 4) in the case of kXR_NotFound a kXR_refresh flags needs to be set
    //--------------------------------------------------------------------------
    if( status.code == errErrorResponse )
    {
      if( RetriableErrorResponse( status ) )
      {
        UpdateTriedCGI(status.errNo);
        if( status.errNo == kXR_NotFound || status.errNo == kXR_Overloaded )
          SwitchOnRefreshFlag();
        HandleError( RetryAtServer( pLoadBalancer.url, RedirectEntry::EntryRetry ) );
        return;
      }
      else
      {
        pStatus = status;
        HandleRspOrQueue();
        return;
      }
    }

    //--------------------------------------------------------------------------
    // Nothing can be done if:
    // 1) a user timeout has occurred
    // 2) has a non-zero session id
    // 3) if another error occurred and the validity of the message expired
    //--------------------------------------------------------------------------
    if( status.code == errOperationExpired || pRequest->GetSessionId() ||
        status.code == errOperationInterrupted || time(0) >= pExpiration )
    {
      log->Error( XRootDMsg, "[%s] Unable to get the response to request %s",
                  pUrl.GetHostId().c_str(),
                  pRequest->GetDescription().c_str() );
      pStatus = status;
      HandleRspOrQueue();
      return;
    }

    //--------------------------------------------------------------------------
    // At this point we're left with connection errors, we recover them
    // at a load balancer if we have one and if not on the current server
    // until we get a response, an unrecoverable error or a timeout
    //--------------------------------------------------------------------------
    if( pLoadBalancer.url.IsValid() &&
        pLoadBalancer.url.GetLocation() != pUrl.GetLocation() )
    {
      UpdateTriedCGI( kXR_ServerError );
      HandleError( RetryAtServer( pLoadBalancer.url, RedirectEntry::EntryRetry ) );
      return;
    }
    else
    {
      if( !status.IsFatal() && IsRetriable() )
      {
        log->Info( XRootDMsg, "[%s] Retrying request: %s.",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        UpdateTriedCGI( kXR_ServerError );
        HandleError( RetryAtServer( pUrl, RedirectEntry::EntryRetry ) );
        return;
      }
      pStatus = status;
      HandleRspOrQueue();
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Retry the message at another server
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::RetryAtServer( const URL &url, RedirectEntry::Type entryType )
  {
    pResponse.reset();
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Set up a redirect entry
    //--------------------------------------------------------------------------
    if( pRdirEntry ) pRedirectTraceBack.push_back( std::move( pRdirEntry ) );
    pRdirEntry.reset( new RedirectEntry( pUrl.GetLocation(), url.GetLocation(), entryType ) );

    if( pUrl.GetLocation() != url.GetLocation() )
    {
      pHosts->push_back( url );

      //------------------------------------------------------------------------
      // Assign a new stream id to the message
      //------------------------------------------------------------------------

      // first release the old stream id
      // (though it could be a redirect from a local
      //  metalink file, in this case there's no SID)
      ClientRequestHdr *req = (ClientRequestHdr*)pRequest->GetBuffer();
      if( pSidMgr )
      {
        pSidMgr->ReleaseSID( req->streamid );
        pSidMgr.reset();
      }

      // then get the new SIDManager
      // (again this could be a redirect to a local
      // file and in this case there is no SID)
      if( !url.IsLocalFile() )
      {
        pSidMgr = SIDMgrPool::Instance().GetSIDMgr( url );
        Status st = pSidMgr->AllocateSID( req->streamid );
        if( !st.IsOK() )
        {
          log->Error( XRootDMsg, "[%s] Impossible to send message %s.",
          pUrl.GetHostId().c_str(),
          pRequest->GetDescription().c_str() );
          return st;
        }
      }

      pUrl = url;
    }

    if( pUrl.IsMetalink() && pFollowMetalink )
    {
      log->Debug( ExDbgMsg, "[%s] Metaling redirection for MsgHandler: 0x%x (message: %s ).",
                  pUrl.GetHostId().c_str(), this,
                  pRequest->GetDescription().c_str() );

      return pPostMaster->Redirect( pUrl, pRequest, this );
    }
    else if( pUrl.IsLocalFile() )
    {
      HandleLocalRedirect( &pUrl );
      return Status();
    }
    else
    {
      log->Debug( ExDbgMsg, "[%s] Retry at server MsgHandler: 0x%x (message: %s ).",
                  pUrl.GetHostId().c_str(), this,
                  pRequest->GetDescription().c_str() );
      return pPostMaster->Send( pUrl, pRequest, this, true, pExpiration );
    }
  }

  //----------------------------------------------------------------------------
  // Update the "tried=" part of the CGI of the current message
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::UpdateTriedCGI(uint32_t errNo)
  {
    URL::ParamsMap cgi;
    std::string    tried;

    //--------------------------------------------------------------------------
    // In case a data server responded with a kXR_redirect and we fail at the
    // node where we were redirected to, the original data server should be
    // included in the tried CGI opaque info (instead of the current one).
    //--------------------------------------------------------------------------
    if( pEffectiveDataServerUrl )
    {
      tried = pEffectiveDataServerUrl->GetHostName();
      delete pEffectiveDataServerUrl;
      pEffectiveDataServerUrl = 0;
    }
    //--------------------------------------------------------------------------
    // Otherwise use the current URL.
    //--------------------------------------------------------------------------
    else
      tried = pUrl.GetHostName();

    // Report the reason for the failure to the next location
    //
    if (errNo)
       {     if (errNo == kXR_NotFound)     cgi["triedrc"] = "enoent";
        else if (errNo == kXR_IOError)      cgi["triedrc"] = "ioerr";
        else if (errNo == kXR_FSError)      cgi["triedrc"] = "fserr";
        else if (errNo == kXR_ServerError)  cgi["triedrc"] = "srverr";
       }

    //--------------------------------------------------------------------------
    // If our current load balancer is a metamanager and we failed either
    // at a diskserver or at an unidentified node we also exclude the last
    // known manager
    //--------------------------------------------------------------------------
    if( pLoadBalancer.url.IsValid() && (pLoadBalancer.flags & kXR_attrMeta) )
    {
      HostList::reverse_iterator it;
      for( it = pHosts->rbegin()+1; it != pHosts->rend(); ++it )
      {
        if( it->loadBalancer )
          break;

        tried += "," + it->url.GetHostName();

        if( it->flags & kXR_isManager )
          break;
      }
    }

    cgi["tried"] = tried;
    XRootDTransport::UnMarshallRequest( pRequest );
    MessageUtils::RewriteCGIAndPath( pRequest, cgi, false, "" );
    XRootDTransport::MarshallRequest( pRequest );
  }

  //----------------------------------------------------------------------------
  // Switch on the refresh flag for some requests
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::SwitchOnRefreshFlag()
  {
    XRootDTransport::UnMarshallRequest( pRequest );
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();
    switch( req->header.requestid )
    {
      case kXR_locate:
      {
        req->locate.options |= kXR_refresh;
        break;
      }

      case kXR_open:
      {
        req->locate.options |= kXR_refresh;
        break;
      }
    }
    XRootDTransport::SetDescription( pRequest );
    XRootDTransport::MarshallRequest( pRequest );
  }

  //------------------------------------------------------------------------
  // If the current thread is a worker thread from our thread-pool
  // handle the response, otherwise submit a new task to the thread-pool
  //------------------------------------------------------------------------
  void XRootDMsgHandler::HandleRspOrQueue()
  {
    JobManager *jobMgr = pPostMaster->GetJobManager();
    if( jobMgr->IsWorker() )
      HandleResponse();
    else
    {
      Log *log = DefaultEnv::GetLog();
      log->Debug( ExDbgMsg, "[%s] Passing to the thread-pool MsgHandler: 0x%x (message: %s ).",
                  pUrl.GetHostId().c_str(), this,
                  pRequest->GetDescription().c_str() );
      jobMgr->QueueJob( new HandleRspJob( this ), 0 );
    }
  }
  
  //------------------------------------------------------------------------
  // Notify the FileStateHandler to retry Open() with new URL
  //------------------------------------------------------------------------
  void XRootDMsgHandler::HandleLocalRedirect( URL *url )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( ExDbgMsg, "[%s] Handling local redirect - MsgHandler: 0x%x (message: %s ).",
                pUrl.GetHostId().c_str(), this,
                pRequest->GetDescription().c_str() );

    if( !pLFileHandler )
    {
      HandleError( XRootDStatus( stFatal, errNotSupported ) );
      return;
    }

    AnyObject *resp = 0;
    pLFileHandler->SetHostList( *pHosts );
    XRootDStatus st = pLFileHandler->Open( url, pRequest, resp );
    if( !st.IsOK() )
    {
      HandleError( st );
      return;
    }

    pResponseHandler->HandleResponseWithHosts( new XRootDStatus(),
                                               resp,
                                               pHosts.release() );
    delete this;

    return;
  }

  //------------------------------------------------------------------------
  // Check if it is OK to retry this request
  //------------------------------------------------------------------------
  bool XRootDMsgHandler::IsRetriable()
  {
    std::string value;
    DefaultEnv::GetEnv()->GetString( "OpenRecovery", value );
    if( value == "true" ) return true;

    // check if it is a mutable open (open + truncate or open + create)
    ClientRequest *req = reinterpret_cast<ClientRequest*>( pRequest->GetBuffer() );
    if( req->header.requestid == htons( kXR_open ) )
    {
      bool _mutable = ( req->open.options & htons( kXR_delete ) ) ||
                      ( req->open.options & htons( kXR_new ) );

      if( _mutable )
      {
        Log *log = DefaultEnv::GetLog();
        log->Debug( XRootDMsg,
                    "[%s] Not allowed to retry open request (OpenRecovery disabled): %s.",
                    pUrl.GetHostId().c_str(),
                    pRequest->GetDescription().c_str() );
        // disallow retry if it is a mutable open
        return false;
      }
    }

    return true;
  }

  //------------------------------------------------------------------------
  // Check if for given request and Metalink redirector  it is OK to omit
  // the kXR_wait and proceed straight to the next entry in the Metalink file
  //------------------------------------------------------------------------
  bool XRootDMsgHandler::OmitWait( Message &request, const URL &url )
  {
    // we can omit kXR_wait only if we have a Metalink redirector
    if( !url.IsMetalink() )
      return false;

    // we can omit kXR_wait only for requests that can be redirected
    // (kXR_read is the only stateful request that can be redirected)
    ClientRequest *req = reinterpret_cast<ClientRequest*>( request.GetBuffer() );
    if( pStateful && req->header.requestid != kXR_read )
      return false;

    // we can only omit kXR_wait if the Metalink redirect has more
    // replicas
    RedirectorRegistry &registry  = RedirectorRegistry::Instance();
    VirtualRedirector *redirector = registry.Get( url );

    // we need more than one server as the current one is not reflected
    // in tried CGI
    if( redirector->Count( request ) > 1 )
      return true;

    return false;
  }

  //------------------------------------------------------------------------
  // Checks if the given error returned by server is retriable.
  //------------------------------------------------------------------------
  bool XRootDMsgHandler::RetriableErrorResponse( const Status &status )
  {
    // we can only retry error response if we have a valid load-balancer and
    // it is not our current URL
    if( !( pLoadBalancer.url.IsValid() &&
           pUrl.GetLocation() != pLoadBalancer.url.GetLocation() ) )
      return false;

    // following errors are retriable at any load-balancer
    if( status.errNo == kXR_FSError     || status.errNo == kXR_IOError  ||
        status.errNo == kXR_ServerError || status.errNo == kXR_NotFound ||
        status.errNo == kXR_Overloaded  || status.errNo == kXR_NoMemory )
      return true;

    // check if the load-balancer is a meta-manager, if yes there are
    // more errors that can be recovered
    if( !( pLoadBalancer.flags & kXR_attrMeta ) ) return false;

    // those errors are retriable for meta-managers
    if( status.errNo == kXR_Unsupported || status.errNo == kXR_FileLocked )
      return true;

    // in case of not-authorized error there is an imposed upper limit
    // on how many times we can retry this error
    if( status.errNo == kXR_NotAuthorized )
    {
      int limit = DefaultNotAuthorizedRetryLimit;
      DefaultEnv::GetEnv()->GetInt( "NotAuthorizedRetryLimit", limit );
      bool ret = pNotAuthorizedCounter < limit;
      ++pNotAuthorizedCounter;
      if( !ret )
      {
        Log *log = DefaultEnv::GetLog();
        log->Error( XRootDMsg,
                    "[%s] Reached limit of NotAuthorized retries!",
                    pUrl.GetHostId().c_str() );
      }
      return ret;
    }

    // check if the load-balancer is a virtual (metalink) redirector,
    // if yes there are even more errors that can be recovered
    if( !( pLoadBalancer.flags & kXR_attrVirtRdr ) ) return false;

    // those errors are retriable for virtual (metalink) redirectors
    if( status.errNo == kXR_noserver || status.errNo == kXR_ArgTooLong )
      return true;

    // otherwise it is a non-retriable error
    return false;
  }

  //------------------------------------------------------------------------
  // Dump the redirect-trace-back into the log file
  //------------------------------------------------------------------------
  void XRootDMsgHandler::DumpRedirectTraceBack()
  {
    if( pRedirectTraceBack.empty() ) return;

    std::stringstream sstrm;

    sstrm << "Redirect trace-back:\n";

    int counter = 0;

    auto itr = pRedirectTraceBack.begin();
    sstrm << '\t' << counter << ". " << (*itr)->ToString() << '\n';

    auto prev = itr;
    ++itr;
    ++counter;

    for( ; itr != pRedirectTraceBack.end(); ++itr, ++prev, ++counter )
      sstrm << '\t' << counter << ". "
            << (*itr)->ToString( (*prev)->status.IsOK() ) << '\n';

    int authlimit = DefaultNotAuthorizedRetryLimit;
    DefaultEnv::GetEnv()->GetInt( "NotAuthorizedRetryLimit", authlimit );

    bool warn = !pStatus.IsOK() &&
              ( pStatus.code == errNotFound ||
                pStatus.code == errRedirectLimit ||
                ( pStatus.code == errAuthFailed && pNotAuthorizedCounter >= authlimit ) );

    Log *log = DefaultEnv::GetLog();
    if( warn )
      log->Warning( XRootDMsg, sstrm.str().c_str() );
    else
      log->Debug( XRootDMsg, sstrm.str().c_str() );
  }
  
  // Read data from buffer
  //------------------------------------------------------------------------
  template<typename T>
  Status XRootDMsgHandler::ReadFromBuffer( char *&buffer, size_t &buflen, T& result )
  {
    if( sizeof( T ) > buflen ) return Status( stError, errDataError );

    memcpy(&result, buffer, sizeof(T));

    buffer += sizeof( T );
    buflen -= sizeof( T );

    return Status();
  }

  //------------------------------------------------------------------------
  // Read a string from buffer
  //------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadFromBuffer( char *&buffer, size_t &buflen, std::string &result )
  {
    Status status;
    char c = 0;

    while( true )
    {
      if( !( status = ReadFromBuffer( buffer, buflen, c ) ).IsOK() )
        return status;

      if( c == 0 ) break;
      result += c;
    }

    return status;
  }

  //------------------------------------------------------------------------
  // Read a string from buffer
  //------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadFromBuffer( char *&buffer, size_t &buflen,
                                           size_t size, std::string &result )
  {
    Status status;

    if( size > buflen ) return Status( stError, errDataError );

    result.append( buffer, size );
    buffer += size;
    buflen -= size;

    return status;
  }

}
