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
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClMessageUtils.hh"

#include <arpa/inet.h>              // for network unmarshalling stuff
#include "XrdSys/XrdSysPlatform.hh" // same as above
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
};

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Examine an incoming message, and decide on the action to be taken
  //----------------------------------------------------------------------------
  uint16_t XRootDMsgHandler::Examine( Message *msg )
  {
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
        return Take;

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
        pResponse = 0;
        pPartialResps.push_back( msg );

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
            return Take | Raw | NoProcess;
          }
          else
          {
            pReadRawCurrentOffset += dlen;
            return Take | NoProcess;
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
            return Take | Raw | NoProcess;
          }
          else
            return Take | NoProcess;
        }

        return Take | NoProcess;
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
      delete msg;
      pResponse = embededMsg; // this can never happen for oksofars

      // we need to unmarshall the header by hand
      XRootDTransport::UnMarshallHeader( embededMsg );
      Process( embededMsg );
      return;
    }

    //--------------------------------------------------------------------------
    // We got an answer, check who we were talking to
    //--------------------------------------------------------------------------
    AnyObject  qryResult;
    int       *qryResponse = 0;
    pPostMaster->QueryTransport( pUrl, XRootDQuery::ServerFlags, qryResult );
    qryResult.Get( qryResponse );
    pHosts->back().flags = *qryResponse; delete qryResponse; qryResponse = 0;
    pPostMaster->QueryTransport( pUrl, XRootDQuery::ProtocolVersion, qryResult );
    qryResult.Get( qryResponse );
    pHosts->back().protocol = *qryResponse; delete qryResponse;

    //--------------------------------------------------------------------------
    // Process the message
    //--------------------------------------------------------------------------
    XRootDTransport::UnMarshallBody( msg, req->header.requestid );
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

        HandleError( Status(stError, errErrorResponse), pResponse );
        return;
      }

      //------------------------------------------------------------------------
      // kXR_redirect - they tell us to go elsewhere
      //------------------------------------------------------------------------
      case kXR_redirect:
      {
        XRDCL_SMART_PTR_T<Message> msgPtr( pResponse );
        pResponse = 0;

        if( rsp->hdr.dlen < 4 )
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
          log->Dump( XRootDMsg, "[%s] Redirect limit has been reached for"
                     "message %s", pUrl.GetHostId().c_str(),
                     pRequest->GetDescription().c_str() );

          pStatus = Status( stFatal, errRedirectLimit );
          HandleResponse();
          return;
        }
        --pRedirectCounter;

        //----------------------------------------------------------------------
        // Keep the info about this server if we still need to find a load
        // balancer
        //----------------------------------------------------------------------
        if( !pHasLoadBalancer )
        {
          uint32_t flags = pHosts->back().flags;
          if( flags & kXR_isManager )
          {
            //------------------------------------------------------------------
            // If the current server is a meta manager then it supersedes
            // any existing load balancer, otherwise we assign a load-balancer
            // only if it has not been already assigned
            //------------------------------------------------------------------
            if( flags & kXR_attrMeta || !pLoadBalancer.url.IsValid() )
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

        pUrl         = newUrl;
        pRedirectUrl = newUrl.GetURL();

        URL cgiURL;
        if( urlComponents.size() > 1 )
        {
          std::ostringstream o;
          o << "fake://fake:111//fake?";
          o << urlComponents[1];
          pRedirectUrl += "?";
          pRedirectUrl += urlComponents[1];
          cgiURL = URL( o.str() );
        }

        //----------------------------------------------------------------------
        // Check if we need to return the URL as a response
        //----------------------------------------------------------------------
        if( pUrl.GetProtocol() != "root" && pUrl.GetProtocol() != "xroot" )
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
        Status st = RewriteRequestRedirect( cgiURL.GetParams(),
                                            pUrl.GetPath() );
        if( !st.IsOK() )
        {
          pStatus = st;
          HandleResponse();
          return;
        }

        //----------------------------------------------------------------------
        // Send the request to the new location
        //----------------------------------------------------------------------
        pHosts->push_back( pUrl );
        pHosts->back().url.SetParams( cgiURL.GetParams() );
        HandleError( RetryAtServer(pUrl) );
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
      // kXR_waitresp - the response will be returned in some seconds
      // as an unsolicited message
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
  uint8_t XRootDMsgHandler::OnStreamEvent( StreamEvent event,
                                           uint16_t    streamNum,
                                           Status      status )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( XRootDMsg, "[%s] Stream event reported for msg %s",
               pUrl.GetHostId().c_str(), pRequest->GetDescription().c_str() );

    if( event == Ready )
      return 0;

    if( streamNum != 0 )
      return 0;

    HandleError( status, 0 );
    return RemoveHandler;
  }

  //----------------------------------------------------------------------------
  // Read message body directly from a socket
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::ReadMessageBody( Message  *msg,
                                            int       socket,
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
                                        int       socket,
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
                                         int       socket,
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
                                         int       socket,
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
  Status XRootDMsgHandler::ReadAsync( int socket, uint32_t &bytesRead )
  {
    char *buffer = pAsyncReadBuffer;
    buffer += pAsyncOffset;
    while( pAsyncOffset < pAsyncReadSize )
    {
      uint32_t toBeRead = pAsyncReadSize - pAsyncOffset;
      int status = ::read( socket, buffer, toBeRead );
      if( status < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) )
        return Status( stOK, suRetry );

      if( status <= 0 )
        return Status( stError, errSocketError, errno );

      pAsyncOffset     += status;
      buffer           += status;
      bytesRead        += status;
    }
    return Status( stOK, suDone );
  }

  //----------------------------------------------------------------------------
  // We're here when we requested sending something over the wire
  // and there has been a status update on this action
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::OnStatusReady( const Message *message,
                                        Status         status )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We were successful, so we now need to listen for a response
    //--------------------------------------------------------------------------
    if( status.IsOK() )
    {
      log->Dump( XRootDMsg, "[%s] Message %s has been successfully sent.",
                 pUrl.GetHostId().c_str(), message->GetDescription().c_str() );
      Status st = pPostMaster->Receive( pUrl, this, pExpiration );
      if( st.IsOK() )
        return;
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
    if( reqId == kXR_write )
      return true;
    return false;
  }

  //----------------------------------------------------------------------------
  // Write the message body
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::WriteMessageBody( int       socket,
                                             uint32_t &bytesRead )
  {
    char     *buffer          = (char*)(*pChunkList)[0].buffer;
    uint32_t  size            = (*pChunkList)[0].length;
    uint32_t  leftToBeWritten = size-pAsyncOffset;

    while( leftToBeWritten )
    {
      //------------------------------------------------------------------------
      // We use send with MSG_NOSIGNAL to avoid SIGPIPEs on Linux
      //------------------------------------------------------------------------
#ifdef __linux__
      int status = ::send( socket, buffer+pAsyncOffset, leftToBeWritten,
                           MSG_NOSIGNAL );
#else
      int status = ::write( socket, buffer+pAsyncOffset, leftToBeWritten );
#endif
      if( status <= 0 )
      {
        //----------------------------------------------------------------------
        // Writing operation would block! So we are done for now, but we will
        // return here
        //----------------------------------------------------------------------
        if( errno == EAGAIN || errno == EWOULDBLOCK )
          return Status( stOK, suRetry );

        //----------------------------------------------------------------------
        // Actual socket error error!
        //----------------------------------------------------------------------
        return Status( stError, errSocketError, errno );
      }
      pAsyncOffset    += status;
      bytesRead       += status;
      leftToBeWritten -= status;
    }

    //--------------------------------------------------------------------------
    // We're done have written the message successfully
    //--------------------------------------------------------------------------
    return Status();
  }

  //----------------------------------------------------------------------------
  // We're here when we got a time event. We needed to re-issue the request
  // in some time in the future, and that moment has arrived
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::WaitDone( time_t )
  {
    HandleError( RetryAtServer(pUrl) );
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
    // Release the stream id
    //--------------------------------------------------------------------------
    ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();
    if( !status->IsOK() && status->code == errOperationExpired )
      pSidMgr->TimeOutSID( req->header.streamid );
    else
      pSidMgr->ReleaseSID( req->header.streamid );

    pResponseHandler->HandleResponseWithHosts( status, response, pHosts );

    //--------------------------------------------------------------------------
    // As much as I hate to say this, we cannot do more, so we commit
    // a suicide... just make sure that this is the last stateful thing
    // we'll ever do
    //--------------------------------------------------------------------------
    delete this;
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
        char *errmsg = new char[rsp->hdr.dlen-3];
        errmsg[rsp->hdr.dlen-4] = 0;
        memcpy( errmsg, rsp->body.error.errmsg, rsp->hdr.dlen-4 );
        st->SetErrorMessage( errmsg );
        delete errmsg;
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

    //--------------------------------------------------------------------------
    // We only handle the kXR_ok responses further down
    //--------------------------------------------------------------------------
    if( rsp->hdr.status != kXR_ok )
      return 0;

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

        if( data->ParseServerResponse( pUrl.GetHostId(), nullBuffer ) == false )
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

  //----------------------------------------------------------------------------
  // Perform the changes to the original request needed by the redirect
  // procedure - allocate new streamid, append redirection data and such
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::RewriteRequestRedirect(
    const URL::ParamsMap &newCgi,
    const std::string    &newPath )
  {
    Log *log = DefaultEnv::GetLog();
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();

    //--------------------------------------------------------------------------
    // Assign a new stream id to the message
    //--------------------------------------------------------------------------
    Status st;
    pSidMgr->ReleaseSID( req->header.streamid );
    pSidMgr = 0;
    AnyObject sidMgrObj;
    st = pPostMaster->QueryTransport( pUrl, XRootDQuery::SIDManager,
                                      sidMgrObj );

    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Impossible to send message %s.",
                  pUrl.GetHostId().c_str(),
                  pRequest->GetDescription().c_str() );
      return st;
    }

    sidMgrObj.Get( pSidMgr );
    st = pSidMgr->AllocateSID( req->header.streamid );
    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Impossible to send message %s.",
                  pUrl.GetHostId().c_str(),
                  pRequest->GetDescription().c_str() );
      return st;
    }

    //--------------------------------------------------------------------------
    // Rewrite particular requests
    //--------------------------------------------------------------------------
    XRootDTransport::UnMarshallRequest( pRequest );
    MessageUtils::RewriteCGIAndPath( pRequest, newCgi, true, newPath );
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
  void XRootDMsgHandler::HandleError( Status status, Message *msg )
  {
    //--------------------------------------------------------------------------
    // If there was no error then do nothing
    //--------------------------------------------------------------------------
    if( status.IsOK() )
      return;

    Log *log = DefaultEnv::GetLog();
    log->Error( XRootDMsg, "[%s] Handling error while processing %s: %s.",
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
      if( pLoadBalancer.url.IsValid() &&
          pUrl.GetHostId() != pLoadBalancer.url.GetHostId() &&
          (status.errNo == kXR_FSError || status.errNo == kXR_IOError ||
          status.errNo == kXR_ServerError || status.errNo == kXR_NotFound) )
      {
        UpdateTriedCGI();
        if( status.errNo == kXR_NotFound )
          SwitchOnRefreshFlag();
        HandleError( RetryAtServer( pLoadBalancer.url ) );
        delete pResponse;
        return;
      }
      else
      {
        pStatus = status;
        HandleResponse();
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
        time(0) >= pExpiration )
    {
      log->Error( XRootDMsg, "[%s] Unable to get the response to request %s",
                  pUrl.GetHostId().c_str(),
                  pRequest->GetDescription().c_str() );
      pStatus = status;
      HandleResponse();
      return;
    }

    //--------------------------------------------------------------------------
    // At this point we're left with connection errors, we recover them
    // at a load balancer if we have one and if not on the current server
    // until we get a response, an unrecoverable error or a timeout
    //--------------------------------------------------------------------------
    if( pLoadBalancer.url.IsValid() &&
        pLoadBalancer.url.GetHostId() != pUrl.GetHostId() )
    {
      UpdateTriedCGI();
      HandleError( RetryAtServer( pLoadBalancer.url ) );
      return;
    }
    else
    {
      if( !status.IsFatal() )
      {
        HandleError( RetryAtServer( pUrl ) );
        return;
      }
      pStatus = status;
      HandleResponse();
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Retry the message at another server
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::RetryAtServer( const URL &url )
  {
    pUrl = url;
    pHosts->push_back( pUrl );
    return pPostMaster->Send( pUrl, pRequest, this, true, pExpiration );
  }

  //----------------------------------------------------------------------------
  // Update the "tried=" part of the CGI of the current message
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::UpdateTriedCGI()
  {
    URL::ParamsMap cgi;
    cgi["tried"] = pUrl.GetHostName();
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
}
