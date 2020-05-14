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
#include "XrdCl/XrdClUglyHacks.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClLocalFileHandler.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClTls.hh"

#include <arpa/inet.h>              // for network unmarshalling stuff
#include "XrdSys/XrdSysPlatform.hh" // same as above
#include "XrdSys/XrdSysAtomics.hh"
#include <memory>
#include <sstream>

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
  uint16_t XRootDMsgHandler::Examine( Message *msg )
  {
    //--------------------------------------------------------------------------
    // if the MsgHandler is already being used to process another request
    // (kXR_oksofar) we need to wait
    //--------------------------------------------------------------------------
    if( pOksofarAsAnswer )
    {
      XrdSysCondVarHelper lck( pCV );
      while( pResponse != 0 ) pCV.Wait();
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
    // We got an async message
    //--------------------------------------------------------------------------
    if( rsp->hdr.status == kXR_attn )
    {
      if( msg->GetSize() < 12 )
        return Ignore;

      //------------------------------------------------------------------------
      // We only care about async responses
      //------------------------------------------------------------------------
      if( rsp->body.attn.actnum != (int32_t)htonl(kXR_asynresp) )
        return Ignore;

      if( msg->GetSize() < 24 )
        return Ignore;

      //------------------------------------------------------------------------
      // Check if the message has the stream ID that we're interested in
      //------------------------------------------------------------------------
      ServerResponse *embRsp = (ServerResponse*)msg->GetBuffer(16);
      if( embRsp->hdr.streamid[0] != req->header.streamid[0] ||
          embRsp->hdr.streamid[1] != req->header.streamid[1] )
        return Ignore;

      status = ntohs( embRsp->hdr.status );
      dlen   = ntohl( embRsp->hdr.dlen );
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

    Log *log = DefaultEnv::GetLog();
    switch( status )
    {
      //------------------------------------------------------------------------
      // Handle the cached cases
      //------------------------------------------------------------------------
      case kXR_error:
      case kXR_redirect:
      case kXR_wait:
        return Take | RemoveHandler;

      case kXR_waitresp:
      {
        log->Dump( XRootDMsg, "[%s] Got kXR_waitresp response to "
                   "message %s", pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

        pResponse = 0;
        return Take | Ignore; // This must be handled synchronously!
      }

      //------------------------------------------------------------------------
      // Handle the potential raw cases
      //------------------------------------------------------------------------
      case kXR_ok:
      {
        //----------------------------------------------------------------------
        // For kXR_read we read in raw mode if we haven't got the full message
        // already (handler installed to late and the message has been cached)
        //----------------------------------------------------------------------
        uint16_t reqId = ntohs( req->header.requestid );
        if( reqId == kXR_read && msg->GetSize() == 8 )
        {
          pReadRawStarted = false;
          pAsyncMsgSize   = dlen;
          return Take | Raw | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // kXR_readv is the same as kXR_read
        //----------------------------------------------------------------------
        if( reqId == kXR_readv  && msg->GetSize() == 8 )
        {
          pAsyncMsgSize      = dlen;
          pReadVRawMsgOffset = 0;
          return Take | Raw | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // For everything else we just take what we got
        //----------------------------------------------------------------------
        return Take | RemoveHandler;
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
          pResponse = 0;
          pPartialResps.push_back( msg );
        }

        //----------------------------------------------------------------------
        // For kXR_read we either read in raw mode if the message has not
        // been fully reconstructed already, if it has, we adjust
        // the buffer offset to prepare for the next one
        //----------------------------------------------------------------------
        uint16_t reqId = ntohs( req->header.requestid );
        if( reqId == kXR_read )
        {
          if( msg->GetSize() == 8 )
          {
            pReadRawStarted = false;
            pAsyncMsgSize   = dlen;
#if __cplusplus >= 201103L
            pTimeoutFence = true;
#else
            AtomicCAS( pTimeoutFence, pTimeoutFence, true );
#endif
            return Take | Raw | ( pOksofarAsAnswer ? 0 : NoProcess );
          }
          else
          {
            pReadRawCurrentOffset += dlen;
            return Take | ( pOksofarAsAnswer ? 0 : NoProcess );
          }
        }

        //----------------------------------------------------------------------
        // kXR_readv is similar to read, except that the payload is different
        //----------------------------------------------------------------------
        if( reqId == kXR_readv )
        {
          if( msg->GetSize() == 8 )
          {
            pAsyncMsgSize      = dlen;
            pReadVRawMsgOffset = 0;
#if __cplusplus >= 201103L
            pTimeoutFence = true;
#else
            AtomicCAS( pTimeoutFence, pTimeoutFence, true );
#endif
            return Take | Raw | ( pOksofarAsAnswer ? 0 : NoProcess );
          }
          else
            return Take | ( pOksofarAsAnswer ? 0 : NoProcess );
        }

        return Take | ( pOksofarAsAnswer ? 0 : NoProcess );
      }

      //------------------------------------------------------------------------
      // Default
      //------------------------------------------------------------------------
      default:
        return Take | RemoveHandler;
    }
    return Take | RemoveHandler;
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
  void XRootDMsgHandler::Process( Message *msg )
  {
    Log *log = DefaultEnv::GetLog();

    ServerResponse *rsp = (ServerResponse *)msg->GetBuffer();
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();

    //--------------------------------------------------------------------------
    // We got an async message
    //--------------------------------------------------------------------------
    if( rsp->hdr.status == kXR_attn )
    {
      log->Dump( XRootDMsg, "[%s] Got an async response to message %s, "
                 "processing it", pUrl.GetHostId().c_str(),
                 pRequest->GetDescription().c_str() );
      Message *embededMsg = new Message( rsp->hdr.dlen-8 );
      embededMsg->Append( msg->GetBuffer( 16 ), rsp->hdr.dlen-8 );
      XRDCL_SMART_PTR_T<Message> msgPtr( msg );
      pResponse = embededMsg; // this can never happen for oksofars

      // we need to unmarshall the header by hand
      XRootDTransport::UnMarshallHeader( embededMsg );

      //------------------------------------------------------------------------
      // Check if the dlen field of the embedded message is consistent with
      // the dlen value of the original message
      //------------------------------------------------------------------------
      ServerResponse *embRsp = (ServerResponse *)embededMsg->GetBuffer();
      if( embRsp->hdr.dlen != rsp->hdr.dlen-16 )
      {
        log->Error( XRootDMsg, "[%s] Sizes of the async response to %s and the "
                    "embedded message are inconsistent. Expected %d, got %d.",
                    pUrl.GetHostId().c_str(), pRequest->GetDescription().c_str(),
                    rsp->hdr.dlen-16, embRsp->hdr.dlen);

        pStatus = Status( stFatal, errInvalidMessage );
        HandleResponse();
        return;
      }

      Process( embededMsg );
      return;
    }

    //--------------------------------------------------------------------------
    // If it is a local file, it can be only a metalink redirector
    //--------------------------------------------------------------------------
    if( pUrl.IsLocalFile() && pUrl.IsMetalink() )
    {
      pHosts->back().flags    = kXR_isManager | kXR_attrMeta | kXR_attrVirtRdr;
      pHosts->back().protocol = kXR_PROTOCOLVERSION;
    }
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
    Status st = XRootDTransport::UnMarshallBody( msg, req->header.requestid );
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
        pStatus   = Status( stOK, suContinue );
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

        HandleError( Status(stError, errErrorResponse, rsp->body.error.errnum),
                     pResponse );
        return;
      }

      //------------------------------------------------------------------------
      // kXR_redirect - they tell us to go elsewhere
      //------------------------------------------------------------------------
      case kXR_redirect:
      {
        XRDCL_SMART_PTR_T<Message> msgPtr( pResponse );
        pResponse = 0;

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
        if( rsp->body.redirect.port != -1 )
          o << ":" << rsp->body.redirect.port << "/";

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
          pResponse = msgPtr.release();
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
        XRDCL_SMART_PTR_T<Message> msgPtr( pResponse );
        pResponse = 0;
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
        if( OmitWait( pRequest, pLoadBalancer.url ) )
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
          pStatus   = Status( stError, errOperationExpired );
          HandleResponse();
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
        XRDCL_SMART_PTR_T<Message> msgPtr( pResponse );
        pResponse = 0;

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
        XRDCL_SMART_PTR_T<Message> msgPtr( pResponse );
        pResponse = 0;
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

    HandleError( status, 0 );
    return RemoveHandler;
  }

  //----------------------------------------------------------------------------
  // Read message body directly from a socket
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadMessageBody( Message  *msg,
                                            Socket   *socket,
                                            uint32_t &bytesRead )
  {
    ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();
    uint16_t reqId = ntohs( req->header.requestid );
    if( reqId == kXR_read )
      return ReadRawRead( msg, socket, bytesRead );

    if( reqId == kXR_readv )
      return ReadRawReadV( msg, socket, bytesRead );

    return ReadRawOther( msg, socket, bytesRead );
  }

  //----------------------------------------------------------------------------
  // Handle a kXR_read in raw mode
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadRawRead( Message  *msg,
                                        Socket   *socket,
                                        uint32_t &bytesRead )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We need to check if we have and overflow, before we start reading
    // anything
    //--------------------------------------------------------------------------
    if( !pReadRawStarted )
    {
      ChunkInfo chunk  = pChunkList->front();
      pAsyncOffset     = 0;
      pAsyncReadSize   = pAsyncMsgSize;
      pAsyncReadBuffer = ((char*)chunk.buffer)+pReadRawCurrentOffset;
      if( pReadRawCurrentOffset + pAsyncMsgSize > chunk.length )
      {
        log->Error( XRootDMsg, "[%s] Overflow data while reading response to %s"
                    ": expected: %d, got %d bytes",
                   pUrl.GetHostId().c_str(), pRequest->GetDescription().c_str(),
                   chunk.length, pReadRawCurrentOffset + pAsyncMsgSize );

        pChunkStatus.front().sizeError = true;
        pOtherRawStarted               = false;
      }
      else
        pReadRawCurrentOffset += pAsyncMsgSize;
      pReadRawStarted = true;
    }

    //--------------------------------------------------------------------------
    // If we have an overflow we discard all the incoming data. We do this
    // instead of just quitting in order to keep the stream sane.
    //--------------------------------------------------------------------------
    if( pChunkStatus.front().sizeError )
      return ReadRawOther( msg, socket, bytesRead );

    //--------------------------------------------------------------------------
    // Read the data
    //--------------------------------------------------------------------------
    return ReadAsync( socket, bytesRead );
  }

  //----------------------------------------------------------------------------
  // Handle a kXR_readv in raw mode
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadRawReadV( Message  *msg,
                                         Socket   *socket,
                                         uint32_t &bytesRead )
  {
    if( pReadVRawMsgOffset == pAsyncMsgSize )
      return Status( stOK, suDone );

    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We've had an error and we are in the discarding mode
    //--------------------------------------------------------------------------
    if( pReadVRawMsgDiscard )
    {
      Status st = ReadAsync( socket, bytesRead );

      if( st.IsOK() && st.code == suDone )
      {
        pReadVRawMsgOffset          += pAsyncReadSize;
        pReadVRawChunkHeaderDone    = false;
        pReadVRawChunkHeaderStarted = false;
        pReadVRawMsgDiscard         = false;
        delete [] pAsyncReadBuffer;

        if( pReadVRawMsgOffset != pAsyncMsgSize )
          st.code = suRetry;

        log->Dump( XRootDMsg, "[%s] ReadRawReadV: Discarded %d bytes, "
                   "current offset: %d/%d", pUrl.GetHostId().c_str(),
                   pAsyncReadSize, pReadVRawMsgOffset, pAsyncMsgSize );
      }
      return st;
    }

    //--------------------------------------------------------------------------
    // Handle chunk header
    //--------------------------------------------------------------------------
    if( !pReadVRawChunkHeaderDone )
    {
      //------------------------------------------------------------------------
      // Set up the header reading
      //------------------------------------------------------------------------
      if( !pReadVRawChunkHeaderStarted )
      {
        pReadVRawChunkHeaderStarted = true;

        //----------------------------------------------------------------------
        // We cannot afford to read the next header from the stream because
        // we will cross the message boundary
        //----------------------------------------------------------------------
        if( pReadVRawMsgOffset + 16 > pAsyncMsgSize )
        {
          uint32_t discardSize = pAsyncMsgSize - pReadVRawMsgOffset;
          log->Error( XRootDMsg, "[%s] ReadRawReadV: No enough data to read "
                      "another chunk header. Discarding %d bytes.",
                      pUrl.GetHostId().c_str(), discardSize );

          pReadVRawMsgDiscard = true;
          pAsyncOffset        = 0;
          pAsyncReadSize      = discardSize;
          pAsyncReadBuffer    = new char[discardSize];
          return Status( stOK, suRetry );
        }

        //----------------------------------------------------------------------
        // We set up reading of the next header
        //----------------------------------------------------------------------
        pAsyncOffset     = 0;
        pAsyncReadSize   = 16;
        pAsyncReadBuffer = (char*)&pReadVRawChunkHeader;
      }

      //------------------------------------------------------------------------
      // Do the reading
      //------------------------------------------------------------------------
      Status st = ReadAsync( socket, bytesRead );

      //------------------------------------------------------------------------
      // Finalize the header and set everything up for the actual buffer
      //------------------------------------------------------------------------
      if( st.IsOK() && st.code == suDone )
      {
        pReadVRawChunkHeaderDone =  true;
        pReadVRawMsgOffset       += 16;

        pReadVRawChunkHeader.rlen   = ntohl( pReadVRawChunkHeader.rlen );
        pReadVRawChunkHeader.offset = ntohll( pReadVRawChunkHeader.offset );

        //----------------------------------------------------------------------
        // Find the buffer corresponding to the chunk
        //----------------------------------------------------------------------
        bool chunkFound = false;
        for( int i = pReadVRawChunkIndex; i < (int)pChunkList->size(); ++i )
        {
          if( (*pChunkList)[i].offset == (uint64_t)pReadVRawChunkHeader.offset &&
              (*pChunkList)[i].length == (uint32_t)pReadVRawChunkHeader.rlen )
          {
            chunkFound = true;
            pReadVRawChunkIndex = i;
            break;
          }
        }

        //----------------------------------------------------------------------
        // If the chunk was no found we discard the chunk
        //----------------------------------------------------------------------
        if( !chunkFound )
        {
          log->Error( XRootDMsg, "[%s] ReadRawReadV: Impossible to find chunk "
                      "buffer corresponding to %d bytes at %ld",
                      pUrl.GetHostId().c_str(), pReadVRawChunkHeader.rlen,
                      pReadVRawChunkHeader.offset );

          uint32_t discardSize = pReadVRawChunkHeader.rlen;
          if( pReadVRawMsgOffset + discardSize > pAsyncMsgSize )
            discardSize = pAsyncMsgSize - pReadVRawMsgOffset;
          pReadVRawMsgDiscard = true;
          pAsyncOffset        = 0;
          pAsyncReadSize      = discardSize;
          pAsyncReadBuffer    = new char[discardSize];

          log->Dump( XRootDMsg, "[%s] ReadRawReadV: Discarding %d bytes",
                     pUrl.GetHostId().c_str(), discardSize );
          return Status( stOK, suRetry );
        }

        //----------------------------------------------------------------------
        // The chunk was found, but reading all the data will cross the message
        // boundary
        //----------------------------------------------------------------------
        if( pReadVRawMsgOffset + pReadVRawChunkHeader.rlen > pAsyncMsgSize )
        {
          uint32_t discardSize = pAsyncMsgSize - pReadVRawMsgOffset;

          log->Error( XRootDMsg, "[%s] ReadRawReadV: Malformed chunk header: "
                      "reading %d bytes from message would cross the message "
                      "boundary, discarding %d bytes.", pUrl.GetHostId().c_str(),
                      pReadVRawChunkHeader.rlen, discardSize );

          pReadVRawMsgDiscard = true;
          pAsyncOffset        = 0;
          pAsyncReadSize      = discardSize;
          pAsyncReadBuffer    = new char[discardSize];
          pChunkStatus[pReadVRawChunkIndex].sizeError = true;
          return Status( stOK, suRetry );
        }

        //----------------------------------------------------------------------
        // We're good
        //----------------------------------------------------------------------
        pAsyncOffset     = 0;
        pAsyncReadSize   = pReadVRawChunkHeader.rlen;
        pAsyncReadBuffer = (char*)(*pChunkList)[pReadVRawChunkIndex].buffer;
      }

      //------------------------------------------------------------------------
      // We've seen a reading error
      //------------------------------------------------------------------------
      if( !st.IsOK() )
        return st;

      //------------------------------------------------------------------------
      // If we are not done reading the header, return back to the event loop.
      //------------------------------------------------------------------------
      if( st.IsOK() && st.code != suDone )
        return st;
    }

    //--------------------------------------------------------------------------
    // Read the body
    //--------------------------------------------------------------------------
    Status st = ReadAsync( socket, bytesRead );

    if( st.IsOK() && st.code == suDone )
    {

      pReadVRawMsgOffset          += pAsyncReadSize;
      pReadVRawChunkHeaderDone    = false;
      pReadVRawChunkHeaderStarted = false;
      pChunkStatus[pReadVRawChunkIndex].done = true;

      log->Dump( XRootDMsg, "[%s] ReadRawReadV: read buffer for chunk %d@%ld",
                 pUrl.GetHostId().c_str(),
                 pReadVRawChunkHeader.rlen, pReadVRawChunkHeader.offset,
                 pReadVRawMsgOffset, pAsyncMsgSize );

      if( pReadVRawMsgOffset < pAsyncMsgSize )
        st.code = suRetry;
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Handle anything other than kXR_read and kXR_readv in raw mode
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadRawOther( Message  *msg,
                                         Socket   *socket,
                                         uint32_t &bytesRead )
  {
    if( !pOtherRawStarted )
    {
      pAsyncOffset     = 0;
      pAsyncReadSize   = pAsyncMsgSize;
      pAsyncReadBuffer = new char[pAsyncMsgSize];
      pOtherRawStarted = true;
    }

    Status st = ReadAsync( socket, bytesRead );

    if( st.IsOK() && st.code == suRetry )
      return st;

    delete [] pAsyncReadBuffer;
    pAsyncReadBuffer = 0;
    pAsyncOffset     = pAsyncReadSize = 0;

    return st;
  }

  //--------------------------------------------------------------------------
  // Read a buffer asynchronously - depends on pAsyncBuffer, pAsyncSize
  // and pAsyncOffset
  //--------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadAsync( Socket *socket, uint32_t &bytesRead )
  {
    char *buffer = pAsyncReadBuffer;
    buffer += pAsyncOffset;
    while( pAsyncOffset < pAsyncReadSize )
    {
      uint32_t toBeRead = pAsyncReadSize - pAsyncOffset;
      int btsRead = 0;

      Status status = socket->Read( buffer, toBeRead, btsRead );

      if( !status.IsOK() || status.code == suRetry )
        return status;

      pAsyncOffset     += btsRead;
      buffer           += btsRead;
      bytesRead        += btsRead;
    }
    return Status( stOK, suDone );
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

      log->Debug( ExDbgMsg, "[%s] Moving MsgHandler: 0x%x (message: %s ) from out-queu to in-queue.",
                  pUrl.GetHostId().c_str(), this,
                  pRequest->GetDescription().c_str() );

      Status st = pPostMaster->Receive( pUrl, this, pExpiration );
      if( st.IsOK() )
      {
        pMsgInFly = true;
        return;
      }
    }

    //--------------------------------------------------------------------------
    // We have failed, recover if possible
    //--------------------------------------------------------------------------
    log->Error( XRootDMsg, "[%s] Impossible to send message %s. Trying to "
                "recover.", pUrl.GetHostId().c_str(),
                message->GetDescription().c_str() );
    HandleError( status, 0 );
  }

  //----------------------------------------------------------------------------
  // Are we a raw writer or not?
  //----------------------------------------------------------------------------
  bool XRootDMsgHandler::IsRaw() const
  {
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();
    uint16_t reqId = ntohs( req->header.requestid );
    if( reqId == kXR_write || reqId == kXR_writev )
      return true;
    return false;
  }

  //----------------------------------------------------------------------------
  // Write the message body
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::WriteMessageBody( Socket   *socket,
                                             uint32_t &bytesWritten )
  {
    size_t size = pChunkList->size();
    for( size_t i = pAsyncChunkIndex ; i < size; ++i )
    {
      char     *buffer          = (char*)(*pChunkList)[i].buffer;
      uint32_t  size            = (*pChunkList)[i].length;
      size_t    leftToBeWritten = size - pAsyncOffset;

      while( leftToBeWritten )
      {
        int bytesWritten = 0;
        Status st = socket->Send( buffer + pAsyncOffset, leftToBeWritten, bytesWritten );
        if( !st.IsOK() || st.code == suRetry ) return st;
        pAsyncOffset    += bytesWritten;
        leftToBeWritten -= bytesWritten;
      }
      //------------------------------------------------------------------------
      // Remember that we have moved to the next chunk, also clear the offset
      // within the buffer as we are going to move to a new one
      //------------------------------------------------------------------------
      ++pAsyncChunkIndex;
      pAsyncOffset = 0;
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
  // Take down the timeout fence
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::TakeDownTimeoutFence()
  {
#if __cplusplus >= 201103L
    pTimeoutFence = false;
#else
    AtomicCAS( pTimeoutFence, pTimeoutFence, false );
#endif
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
      if( !status->IsOK() && pMsgInFly &&
          ( status->code == errOperationExpired || status->code == errOperationInterrupted ) )
        pSidMgr->TimeOutSID( req->header.streamid );
      else
        pSidMgr->ReleaseSID( req->header.streamid );
    }

    HostList *hosts = pHosts;
    if( !finalrsp )
      pHosts = new HostList( *hosts );

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
      delete pResponse;
      pResponse = 0;
#if __cplusplus >= 201103L
      pTimeoutFence = false;
#else
      AtomicCAS( pTimeoutFence, pTimeoutFence, false );
#endif
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
        std::string errmsg( rsp->body.error.errmsg, rsp->hdr.dlen-4 );
        if( st->errNo == kXR_noReplicas )
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

        //----------------------------------------------------------------------
        // Glue in the cached responses if necessary
        //----------------------------------------------------------------------
        ChunkInfo  chunk         = pChunkList->front();
        bool       sizeMismatch  = false;
        uint32_t   currentOffset = 0;
        char      *cursor        = (char*)chunk.buffer;
        for( uint32_t i = 0; i < pPartialResps.size(); ++i )
        {
          ServerResponse *part = (ServerResponse*)pPartialResps[i]->GetBuffer();

          if( currentOffset + part->hdr.dlen > chunk.length )
          {
            sizeMismatch = true;
            break;
          }

          if( pPartialResps[i]->GetSize() > 8 )
            memcpy( cursor, part->body.buffer.data, part->hdr.dlen );
          currentOffset += part->hdr.dlen;
          cursor        += part->hdr.dlen;
        }

        if( currentOffset + rsp->hdr.dlen <= chunk.length )
        {
          if( pResponse->GetSize() > 8 )
            memcpy( cursor, rsp->body.buffer.data, rsp->hdr.dlen );
          currentOffset += rsp->hdr.dlen;
        }
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

        AnyObject *obj      = new AnyObject();
        ChunkInfo *retChunk = new ChunkInfo( chunk.offset, currentOffset,
                                             chunk.buffer );
        obj->Set( retChunk );
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

        VectorReadInfo *info = new VectorReadInfo();
        Status st = PostProcessReadV( info );
        if( !st.IsOK() )
        {
          delete info;
          return st;
        }

        AnyObject *obj = new AnyObject();
        obj->Set( info );
        response = obj;
        return Status();
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
  // Post process vector read
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::PostProcessReadV( VectorReadInfo *vReadInfo )
  {
    //--------------------------------------------------------------------------
    // Unpack the stuff that needs to be unpacked
    //--------------------------------------------------------------------------
    for( uint32_t i = 0; i < pPartialResps.size(); ++i )
      if( pPartialResps[i]->GetSize() != 8 )
        UnPackReadVResponse( pPartialResps[i] );

    if( pResponse->GetSize() != 8 )
      UnPackReadVResponse( pResponse );

    //--------------------------------------------------------------------------
    // See if all the chunks are OK and put them in the response
    //--------------------------------------------------------------------------
    uint32_t size = 0;
    for( uint32_t i = 0; i < pChunkList->size(); ++i )
    {
      if( !pChunkStatus[i].done )
        return Status( stFatal, errInvalidResponse );

      vReadInfo->GetChunks().push_back(
                      ChunkInfo( (*pChunkList)[i].offset,
                                 (*pChunkList)[i].length,
                                 (*pChunkList)[i].buffer ) );
      size += (*pChunkList)[i].length;
    }
    vReadInfo->SetSize( size );
    return Status();
  }

  //----------------------------------------------------------------------------
  //! Unpack a single readv response
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::UnPackReadVResponse( Message *msg )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( XRootDMsg, "[%s] Handling response to %s: unpacking "
               "data from a cached message", pUrl.GetHostId().c_str(),
               pRequest->GetDescription().c_str() );

    uint32_t  offset       = 0;
    uint32_t  len          = msg->GetSize()-8;
    uint32_t  currentChunk = 0;
    char     *cursor       = msg->GetBuffer(8);

    while( 1 )
    {
      //------------------------------------------------------------------------
      // Check whether we should stop
      //------------------------------------------------------------------------
      if( offset+16 > len )
        break;

      //------------------------------------------------------------------------
      // Extract and check the validity of the chunk
      //------------------------------------------------------------------------
      readahead_list *chunk = (readahead_list*)(cursor);
      chunk->rlen   = ntohl( chunk->rlen );
      chunk->offset = ntohll( chunk->offset );

      bool chunkFound = false;
      for( uint32_t i = currentChunk; i < pChunkList->size(); ++i )
      {
        if( (*pChunkList)[i].offset == (uint64_t)chunk->offset &&
            (*pChunkList)[i].length == (uint32_t)chunk->rlen )
        {
          chunkFound   = true;
          currentChunk = i;
          break;
        }
      }

      if( !chunkFound )
      {
        log->Error( XRootDMsg, "[%s] Handling response to %s: the response "
                    "no corresponding chunk buffer found to store %d bytes "
                    "at %ld", pUrl.GetHostId().c_str(),
                    pRequest->GetDescription().c_str(), chunk->rlen,
                    chunk->offset );
        return Status( stFatal, errInvalidResponse );
      }

      //------------------------------------------------------------------------
      // Extract the data
      //------------------------------------------------------------------------
      if( !(*pChunkList)[currentChunk].buffer )
      {
        log->Error( XRootDMsg, "[%s] Handling response to %s: the user "
                    "supplied buffer is 0, discarding the data",
                    pUrl.GetHostId().c_str(),
                    pRequest->GetDescription().c_str() );
      }
      else
      {
        if( offset+chunk->rlen+16 > len )
        {
          log->Error( XRootDMsg, "[%s] Handling response to %s: copying "
                      "requested data would cross message boundary",
                      pUrl.GetHostId().c_str(),
                      pRequest->GetDescription().c_str() );
          return Status( stFatal, errInvalidResponse );
        }
        memcpy( (*pChunkList)[currentChunk].buffer, cursor+16, chunk->rlen );
      }

      pChunkStatus[currentChunk].done = true;

      offset += (16 + chunk->rlen);
      cursor += (16 + chunk->rlen);
      ++currentChunk;
    }
    return Status();
  }

  //----------------------------------------------------------------------------
  // Recover error
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::HandleError( XRootDStatus status, Message *msg )
  {
    //--------------------------------------------------------------------------
    // If there was no error then do nothing
    //--------------------------------------------------------------------------
    if( status.IsOK() )
      return;

    bool noreplicas = ( status.code == errErrorResponse &&
                        status.errNo == kXR_noReplicas );

    if( !noreplicas ) pLastError = status;

    Log *log = DefaultEnv::GetLog();
    log->Debug( XRootDMsg, "[%s] Handling error while processing %s: %s.",
                pUrl.GetHostId().c_str(), pRequest->GetDescription().c_str(),
                status.ToString().c_str() );

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
        delete pResponse;
        pResponse = 0;
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
      UpdateTriedCGI();
      HandleError( RetryAtServer( pLoadBalancer.url, RedirectEntry::EntryRetry ) );
      return;
    }
    else
    {
      if( !status.IsFatal() && IsRetriable( pRequest ) )
      {
        log->Info( XRootDMsg, "[%s] Retrying request: %s.",
                   pUrl.GetHostId().c_str(),
                   pRequest->GetDescription().c_str() );

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
                                               pHosts );
    delete this;

    return;
  }

  //------------------------------------------------------------------------
  // Check if it is OK to retry this request
  //------------------------------------------------------------------------
  bool XRootDMsgHandler::IsRetriable( Message *request )
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
  bool XRootDMsgHandler::OmitWait( Message *request, const URL &url )
  {
    // we can omit kXR_wait only if we have a Metalink redirector
    if( !url.IsMetalink() )
      return false;

    // we can omit kXR_wait only for requests that can be redirected
    // (kXR_read is the only stateful request that can be redirected)
    ClientRequest *req = reinterpret_cast<ClientRequest*>( request->GetBuffer() );
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

    result = *reinterpret_cast<T*>( buffer );

    buffer += sizeof( T );
    buflen   -= sizeof( T );

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
