//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClSIDManager.hh"


#include <arpa/inet.h>              // for network unmarshalling stuff
#include "XrdSys/XrdSysPlatform.hh" // same as above
#include <memory>

namespace
{
  //----------------------------------------------------------------------------
  // We need an extra task what will run the handler in the future, because
  // tasks get deleted and we need the handler
  //----------------------------------------------------------------------------
  class WaitTask: public XrdClient::Task
  {
    public:
      WaitTask( XrdClient::XRootDMsgHandler *handler ): pHandler( handler ) {}
      virtual time_t Run( time_t now )
      {
        pHandler->WaitDone( now );
        return 0;
      }
    private:
      XrdClient::XRootDMsgHandler *pHandler;
  };
};

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Examine an incomming message, and decide on the action to be taken
  //----------------------------------------------------------------------------
  uint8_t XRootDMsgHandler::HandleMessage( Message *msg )
  {
    Log *log = DefaultEnv::GetLog();

    ServerResponse *rsp = (ServerResponse *)msg->GetBuffer();
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();

    //--------------------------------------------------------------------------
    // We got an async message
    //--------------------------------------------------------------------------
    if( rsp->hdr.status == kXR_attn )
    {
      //------------------------------------------------------------------------
      // We only care about async responses
      //------------------------------------------------------------------------
      if( rsp->body.attn.actnum != (int32_t)htonl(kXR_asynresp) )
        return Ignore;

      //------------------------------------------------------------------------
      // Check if the message has the stream ID that we're interested in
      //------------------------------------------------------------------------
      ServerResponse *embRsp = (ServerResponse*)msg->GetBuffer(16);
      if( embRsp->hdr.streamid[0] != req->header.streamid[0] ||
          embRsp->hdr.streamid[1] != req->header.streamid[1] )
        return Ignore;

      //------------------------------------------------------------------------
      // OK, it looks like we care
      //------------------------------------------------------------------------
      log->Dump( XRootDMsg, "[%s] Got an async response to message 0x%x, "
                            "processing it",
                            pUrl.GetHostId().c_str(), pRequest );
      Message *embededMsg = new Message( rsp->hdr.dlen-8 );
      embededMsg->Append( msg->GetBuffer( 16 ), rsp->hdr.dlen-8 );
      // we need to unmarshall the header by hand
      XRootDTransport::UnMarshallHeader( embededMsg );
      delete msg;
      return HandleMessage( embededMsg );
    }

    //--------------------------------------------------------------------------
    // The message is not async, check if it belongs to us
    //--------------------------------------------------------------------------
    if( rsp->hdr.streamid[0] != req->header.streamid[0] ||
        rsp->hdr.streamid[1] != req->header.streamid[1] )
      return Ignore;

    std::auto_ptr<Message> msgPtr( msg );

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
        log->Dump( XRootDMsg, "[%s] Got a kXR_ok response to request 0x%x",
                             pUrl.GetHostId().c_str(), pRequest );
        pResponse = msgPtr.release();
        pStatus   = Status();
        HandleResponse();
        return Take | RemoveHandler;
      }

      //------------------------------------------------------------------------
      // kXR_error - we've got a problem
      //------------------------------------------------------------------------
      case kXR_error:
      {
        log->Dump( XRootDMsg, "[%s] Got a kXR_error response to request 0x%x: "
                             "[%d] %s",
                             pUrl.GetHostId().c_str(), pRequest,
                             rsp->body.error.errnum, rsp->body.error.errmsg );
        pResponse = msgPtr.release();
        pStatus = Status( stError, errErrorResponse );
        HandleResponse();
        return Take | RemoveHandler;
      }

      //------------------------------------------------------------------------
      // kXR_redirect - they tell us to go elsewhere
      //------------------------------------------------------------------------
      case kXR_redirect:
      {
        char *urlInfoBuff = new char[rsp->hdr.dlen-3];
        urlInfoBuff[rsp->hdr.dlen-4] = 0;
        memcpy( urlInfoBuff, rsp->body.redirect.host, rsp->hdr.dlen-4 );
        std::string urlInfo = urlInfoBuff;
        delete [] urlInfoBuff;
        log->Dump( XRootDMsg, "[%s] Got kXR_redirect response to "
                             "message 0x%x: %s, port %d",
                             pUrl.GetHostId().c_str(), pRequest,
                             urlInfo.c_str(), rsp->body.redirect.port );

        //----------------------------------------------------------------------
        // Check the validity of the url
        //----------------------------------------------------------------------
        pUrl = URL( urlInfo, rsp->body.redirect.port );

        if( !pUrl.IsValid() )
        {
          pStatus = Status( stError, errInvalidRedirectURL );
          log->Error( XRootDMsg, "[%s] Got invalid redirection URL: %s",
                                pUrl.GetHostId().c_str(), urlInfo.c_str() );
          HandleResponse();
          return Take | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // Check if we need to return the URL as a response
        //----------------------------------------------------------------------
        if( pRedirectAsAnswer )
        {
          pStatus = Status( stOK, suXRDRedirect );
          pResponse = msgPtr.release();
          HandleResponse();
          return Take | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // Rewrite the message in a way required to send it to another server
        //----------------------------------------------------------------------
        Status st = RewriteRequestRedirect();
        if( !st.IsOK() )
        {
          pStatus = st;
          HandleResponse();
          return Take | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // Send the request to the new location
        //----------------------------------------------------------------------
        pRedirections->push_back( pUrl );
        st = pPostMaster->Send( pUrl, pRequest, this, 300 );

        if( !st.IsOK() )
        {
          log->Error( XRootDMsg, "[%s] Unable to redirect message 0x%x to: %s",
                                pUrl.GetHostId().c_str(), pRequest,
                                urlInfo.c_str() );
          pStatus = st;
          HandleResponse();
          return Take | RemoveHandler;
        }
        return Take | RemoveHandler;
      }

      //------------------------------------------------------------------------
      // kXR_wait - we wait, and re-issue the request later
      //------------------------------------------------------------------------
      case kXR_wait:
      {
        char *infoMsg = new char[rsp->hdr.dlen-3];
        infoMsg[rsp->hdr.dlen-4] = 0;
        memcpy( infoMsg, rsp->body.wait.infomsg, rsp->hdr.dlen-4 );
        log->Dump( XRootDMsg, "[%s] Got kXR_wait response of %d seconds to "
                             "message 0x%x: %s",
                             pUrl.GetHostId().c_str(), rsp->body.wait.seconds,
                             pRequest, infoMsg );
        delete [] infoMsg;

        //----------------------------------------------------------------------
        // Some messages require rewriting before they can be sent again
        // after wait
        //----------------------------------------------------------------------
        Status st = RewriteRequestWait();
        if( !st.IsOK() )
        {
          pStatus = st;
          HandleResponse();
          return Take | RemoveHandler;
        }

        //----------------------------------------------------------------------
        // Register a task to resend the message in some seconds
        //----------------------------------------------------------------------
        TaskManager *taskMgr = pPostMaster->GetTaskManager();
        taskMgr->RegisterTask( new WaitTask( this ),
                               ::time(0)+rsp->body.wait.seconds );
        return Take | RemoveHandler;
      }

      //------------------------------------------------------------------------
      // kXR_waitresp - the response will be returned in some seconds
      // as an unsolicited message
      //------------------------------------------------------------------------
      case kXR_waitresp:
      {
        log->Dump( XRootDMsg, "[%s] Got kXR_waitresp response of %d seconds to "
                             "message 0x%x",
                             pUrl.GetHostId().c_str(),
                             rsp->body.waitresp.seconds, pRequest );

        // FIXME: we have to think of taking into account the new timeout value
        return Take;
      }

      //------------------------------------------------------------------------
      // We've got a partial answer. Wait for more
      //------------------------------------------------------------------------
      case kXR_oksofar:
      {
        log->Dump( XRootDMsg, "[%s] Got a kXR_oksofar response to request "
                              "0x%x",
                              pUrl.GetHostId().c_str(), pRequest );
        pPartialResps.push_back( msgPtr.release() );
        return Take;
      }

      //------------------------------------------------------------------------
      // Default - unrecognized/unsupported response, declare an error
      //------------------------------------------------------------------------
      default:
      {
        log->Dump( XRootDMsg, "[%s] Got unrecognized response %d to "
                             "message 0x%x",
                             pUrl.GetHostId().c_str(),
                             rsp->hdr.status,
                             pRequest );
        pStatus   = Status( stError, errInvalidResponse );
        HandleResponse();
        return Take | RemoveHandler;
      }
    }

    return Ignore;
  }

  //----------------------------------------------------------------------------
  // Handle an event other that a message arrival - may be timeout
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::HandleFault( Status status )
  {
    Log *log = DefaultEnv::GetLog();
    log->Error( XRootDMsg, "[%s] Unable to get the response to request 0x%x",
                          pUrl.GetHostId().c_str(), pRequest );
    pStatus = status;
    HandleResponse();
  }

  //----------------------------------------------------------------------------
  // We're here when we requested sending something over the wire
  // and there has been a status update on this action
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::HandleStatus( const Message *message,
                                       Status         status )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We were successfull, so we now need to listen for a response
    //--------------------------------------------------------------------------
    if( status.IsOK() )
    {
      log->Dump( XRootDMsg, "[%s] Message 0x%x has been successfully sent.",
                           pUrl.GetHostId().c_str(), message );
      Status st = pPostMaster->Receive( pUrl, this, 300 );
      if( st.IsOK() )
        return;
    }

    log->Error( XRootDMsg, "[%s] Impossible to send message 0x%x.",
                          pUrl.GetHostId().c_str(), message );
    pStatus = status;
    HandleResponse();
  }

  //----------------------------------------------------------------------------
  // We're here when we got a time event. We needed to re-issue the request
  // in some time in the future, and that moment has arrived
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::WaitDone( time_t )
  {
    Log *log = DefaultEnv::GetLog();
    Status st = pPostMaster->Send( pUrl, pRequest, this, 300 );
    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Impossible to send message 0x%x after wait.",
                            pUrl.GetHostId().c_str(), pRequest );
      pStatus = st;
      HandleResponse();
    }
  }

  //----------------------------------------------------------------------------
  // Unpack the message and call the response handler
  //----------------------------------------------------------------------------
  void XRootDMsgHandler::HandleResponse()
  {
    //--------------------------------------------------------------------------
    // Process the response and notify the listener
    //--------------------------------------------------------------------------
    XRootDStatus *status   = ProcessStatus();
    AnyObject    *response = 0;

    if( status->IsOK() )
      response = ParseResponse();
    pResponseHandler->HandleResponse( status, response, pRedirections );

    //--------------------------------------------------------------------------
    // Release the stream id
    //--------------------------------------------------------------------------
    ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();
    pSidMgr->ReleaseSID( req->header.streamid );

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
    if( !pStatus.IsOK() && pStatus.code == errErrorResponse && rsp )
    {
      st->errNo = rsp->body.error.errnum;
      st->SetErrorMessage( rsp->body.error.errmsg );
    }
    return st;
  }

  //------------------------------------------------------------------------
  // Parse the response and put it in an object that could be passed to
  // the user
  //------------------------------------------------------------------------
  AnyObject *XRootDMsgHandler::ParseResponse()
  {
    ServerResponse *rsp = (ServerResponse *)pResponse->GetBuffer();
    ClientRequest  *req = (ClientRequest *)pRequest->GetBuffer();
    Log            *log = DefaultEnv::GetLog();

    XRootDTransport::UnMarshallRequest( pRequest );

    //--------------------------------------------------------------------------
    // Handle redirect as an answer
    //--------------------------------------------------------------------------
    if( rsp->hdr.status == kXR_redirect )
    {
      if( !pRedirectAsAnswer )
      {
        log->Error( XRootDMsg, "Internal Error: trying to pass redirect as an "
                               "answer even though this has never been "
                               "requested" );
        return 0;
      }
      log->Dump( XRootDMsg, "Returning the redirection url as a response to "
                            "0x%x",
                             pRequest );
      AnyObject *obj = new AnyObject();
      URL       *url = new URL( pUrl );
      obj->Set( url );
      return obj;
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
    else
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
        return 0;

      //------------------------------------------------------------------------
      // kXR_locate
      //------------------------------------------------------------------------
      case kXR_locate:
      {
        AnyObject *obj = new AnyObject();
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as "
                             "LocateInfo: %s",
                             pUrl.GetHostId().c_str(), pRequest, buffer );
        LocationInfo *data = new LocationInfo( buffer );
        obj->Set( data );
        return obj;
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
          log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as "
                                "StatInfoVFS",
                                pUrl.GetHostId().c_str(), pRequest );

          StatInfoVFS *data = new StatInfoVFS( buffer );
          obj->Set( data );
        }
        //----------------------------------------------------------------------
        // Normal stat
        //----------------------------------------------------------------------
        else
        {
          log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as StatInfo",
                                pUrl.GetHostId().c_str(), pRequest );

          StatInfo *data = new StatInfo( buffer );
          obj->Set( data );
        }

        return obj;
      }

      //------------------------------------------------------------------------
      // kXR_protocol
      //------------------------------------------------------------------------
      case kXR_protocol:
      {
        AnyObject *obj = new AnyObject();
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as ProtocolInfo",
                             pUrl.GetHostId().c_str(), pRequest );

        ProtocolInfo *data = new ProtocolInfo( rsp->body.protocol.pval,
                                               rsp->body.protocol.flags );
        obj->Set( data );
        return obj;
      }

      //------------------------------------------------------------------------
      // kXR_dirlist
      //------------------------------------------------------------------------
      case kXR_dirlist:
      {
        AnyObject *obj = new AnyObject();
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as "
                              "DirectoryList",
                              pUrl.GetHostId().c_str(), pRequest );

        char *path = new char[req->dirlist.dlen+1];
        path[req->dirlist.dlen] = 0;
        memcpy( path, pRequest->GetBuffer(24), req->dirlist.dlen );
        DirectoryList *data = new DirectoryList( pUrl.GetHostId(), path,
                                                 length ? buffer : 0 );
        delete [] path;
        obj->Set( data );
        return obj;
      }

      //------------------------------------------------------------------------
      // kXR_open - if we got the statistics, otherwise return 0
      //------------------------------------------------------------------------
      case kXR_open:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as OpenInfo",
                              pUrl.GetHostId().c_str(), pRequest );

        AnyObject *obj      = new AnyObject();
        StatInfo  *statInfo = 0;

        if( req->open.options & kXR_retstat )
        {
          log->Dump( XRootDMsg, "[%s] Found StatInfo in response to 0x%x",
                                pUrl.GetHostId().c_str(), pRequest );
          statInfo = new StatInfo( buffer+12 );
        }

        OpenInfo *data = new OpenInfo( (uint8_t*)buffer, statInfo );
        obj->Set( data );
        return obj;
      }

      //------------------------------------------------------------------------
      // kXR_read - we need to pass the length of the buffer to the user code
      //------------------------------------------------------------------------
      case kXR_read:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as int*",
                              pUrl.GetHostId().c_str(), pRequest );

        if( pUserBufferSize < length )
        {
          log->Error( XRootDMsg, "[%s] Handling response to 0x%x: user "
                                 "supplied buffer is to small: %d bytes; got "
                                 "%d bytes of response data",
                                 pUrl.GetHostId().c_str(), pRequest,
                                 pUserBufferSize, length );
          return 0;
        }
        memcpy( pUserBuffer, buffer, length );

        AnyObject *obj = new AnyObject();
        uint32_t  *len = new uint32_t;
        *len = length;
        obj->Set( len );
        return obj;
      }

      //------------------------------------------------------------------------
      // kXR_readv - we need to pass the length of the buffer to the user code
      //------------------------------------------------------------------------
      case kXR_readv:
      {
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as "
                              "VectorReadInfo",
                              pUrl.GetHostId().c_str(), pRequest );

        VectorReadInfo *info = new VectorReadInfo();
        UnpackVectorRead( info, pUserBuffer, pUserBufferSize, buffer, length );

        AnyObject *obj = new AnyObject();
        obj->Set( info );
        return obj;
      }

      //------------------------------------------------------------------------
      // kXR_query
      //------------------------------------------------------------------------
      case kXR_query:
      default:
      {
        AnyObject *obj = new AnyObject();
        log->Dump( XRootDMsg, "[%s] Parsing the response to 0x%x as BinaryData",
                              pUrl.GetHostId().c_str(), pRequest );

        BinaryDataInfo *data = new BinaryDataInfo();
        data->Allocate( length );
        data->Append( buffer, length );
        obj->Set( data );
        return obj;
      }
    };
    return 0;
  }

  //----------------------------------------------------------------------------
  // Perform the changes to the original request needed by the redirect
  // procedure - allocate new streamid, append redirection data and such
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::RewriteRequestRedirect()
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
      log->Error( XRootDMsg, "[%s] Impossible to send message 0x%x.",
                            pUrl.GetHostId().c_str(), pRequest );
      return st;
    }

    sidMgrObj.Get( pSidMgr );
    st = pSidMgr->AllocateSID( req->header.streamid );
    if( !st.IsOK() )
    {
      log->Error( XRootDMsg, "[%s] Impossible to send message 0x%x.",
                            pUrl.GetHostId().c_str(), pRequest );
      return st;
    }
    return Status();
  }

  //----------------------------------------------------------------------------
  // Some requests need to be rewriten also after getting kXR_wait - sigh
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::RewriteRequestWait()
  {
    ClientRequest *req = (ClientRequest *)pRequest->GetBuffer();

    XRootDTransport::UnMarshallRequest( pRequest );

    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // For kXR_locate request the kXR_refresh bit needs to be turned off
      // on wait
      //------------------------------------------------------------------------
      case kXR_locate:
      {
        uint16_t refresh = kXR_refresh;
        req->locate.options &= (~refresh);
        break;
      }
    }

    XRootDTransport::MarshallRequest( pRequest );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Unpack vector read - crazy stuff
  //----------------------------------------------------------------------------
  Status XRootDMsgHandler::UnpackVectorRead( VectorReadInfo *vReadInfo,
                                             char           *targetBuffer,
                                             uint32_t        targetBufferSize,
                                             char           *sourceBuffer,
                                             uint32_t        sourceBufferSize )
  {
    Log *log = DefaultEnv::GetLog();
    int64_t   len          = sourceBufferSize;
    uint32_t  offset       = 0;
    char     *cursorSource = sourceBuffer;
    char     *cursorTarget = targetBuffer;
    uint32_t  size         = 0;

    while( 1 )
    {
      if( offset > len-16 )
        break;

      readahead_list *chunk = (readahead_list*)(cursorSource);
      chunk->rlen   = ntohl( chunk->rlen );
      chunk->offset = ntohll( chunk->offset );
      size += chunk->rlen;
      vReadInfo->GetChunks().push_back( Chunk( chunk->offset, chunk->rlen ) );

      if( size > targetBufferSize )
      {
        log->Error( XRootDMsg, "[%s] Handling response to 0x%x: user "
                               "supplied buffer is to small: %d bytes; got "
                               "%d bytes of response data",
                               pUrl.GetHostId().c_str(), pRequest,
                               targetBufferSize, size );
        return Status( stError, errInvalidResponse );;
      }

      memcpy( cursorTarget, cursorSource+16, chunk->rlen );
      cursorTarget += chunk->rlen;
      offset += 16 + chunk->rlen;
      cursorSource = sourceBuffer+offset;
    }
    vReadInfo->SetSize( size );
    return Status();
  }
}
