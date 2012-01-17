//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Read a message
  //----------------------------------------------------------------------------
  Status XRootDTransport::GetMessage( Message *message, Socket *socket )
  {
    int      sock         = socket->GetFD();
    uint32_t leftToBeRead = 0;

    //--------------------------------------------------------------------------
    // A new message - allocate the space needed for the header
    //--------------------------------------------------------------------------
    if( message->GetCursor() == 0 && message->GetSize() < 8 )
      message->Allocate( 8 );

    //--------------------------------------------------------------------------
    // Read the message header
    //--------------------------------------------------------------------------
    if( message->GetCursor() < 8 )
    {
      leftToBeRead = 8-message->GetCursor();
      while( leftToBeRead )
      {
        int status = ::read( sock, message->GetBufferAtCursor(), leftToBeRead );
        if( status < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) )
          return Status( stOK, suRetry );

        if( status <= 0 )
          return Status( stError, errSocketError, errno );

        leftToBeRead -= status;
        message->AdvanceCursor( status );
      }
      UnMarshallHeader( message );
      message->ReAllocate( *(uint32_t*)(message->GetBuffer(4)) + 8 );
      ServerResponse *resp = (ServerResponse *)message->GetBuffer();
    }

    //--------------------------------------------------------------------------
    // Retrieve the body
    //--------------------------------------------------------------------------
    uint32_t bodySize = *(uint32_t*)(message->GetBuffer(4));
    leftToBeRead      = bodySize-(message->GetCursor()-8);

    while( leftToBeRead )
    {
      int status = ::read( sock, message->GetBufferAtCursor(), leftToBeRead );
      if( status < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) )
        return Status( stOK, suRetry );

      if( status <= 0 )
        return Status( stError, errSocketError, errno );

      leftToBeRead -= status;
      message->AdvanceCursor( status );
    }

    return Status();
  }

  //----------------------------------------------------------------------------
  // Initialize channel
  //----------------------------------------------------------------------------
  void XRootDTransport::InitializeChannel( AnyObject &channelData )
  {
    channelData.Set( new XRootDChannelInfo() );
  }

  //----------------------------------------------------------------------------
  // Finalize channel
  //----------------------------------------------------------------------------
  void XRootDTransport::FinalizeChannel( AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    delete info;
  }

  //----------------------------------------------------------------------------
  // HandShake
  //----------------------------------------------------------------------------
  Status XRootDTransport::HandShake( HandShakeData *handShakeData,
                                     AnyObject     &channelData )
  {
    Log *log = Utils::GetDefaultLog();

    XRootDChannelInfo *info = 0;
    channelData.Get( info );

    switch( handShakeData->step )
    {
      //------------------------------------------------------------------------
      // First step - we need to create and initial handshake and send it out
      //------------------------------------------------------------------------
      case 0:
      {
        handShakeData->out = GenerateInitialHS( handShakeData, info );
        return Status( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      // Second step - we got the reply message to the initial handshake
      //------------------------------------------------------------------------
      case 1:
        return ProcessServerHS( handShakeData, info );

      //------------------------------------------------------------------------
      // Third step - we got the response to the protocol request, we need
      // to process it and send out a login request
      //------------------------------------------------------------------------
      case 2:
      {
        Status st =  ProcessProtocolResp( handShakeData, info );

        if( !st.IsOK() )
          return st;

        handShakeData->out = GenerateLogIn( handShakeData, info );
        return Status( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      // Fourth step - handle the log in response
      //------------------------------------------------------------------------
      case 3:
        return ProcessLogInResp( handShakeData, info );
    };

    return Status( stOK, suDone );
  }

  //----------------------------------------------------------------------------
  // Check if the stream should be disconnected
  //----------------------------------------------------------------------------
  bool XRootDTransport::IsStreamTTLElapsed( time_t     inactiveTime,
                                            AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );

    Env *env = DefaultEnv::GetEnv();

    //--------------------------------------------------------------------------
    // We're connected to a data server
    //--------------------------------------------------------------------------
    if( info->serverFlags & kXR_isServer )
    {
      int ttl = DefaultDataServerTTL;
      env->GetInt( "DataServerTTL", ttl );
      if( inactiveTime >= ttl ) return true;
      return false;
    }

    //--------------------------------------------------------------------------
    // We're connected to a manager
    //--------------------------------------------------------------------------
    if( info->serverFlags & kXR_isManager )
    {
      int ttl = DefaultManagerTTL;
      env->GetInt( "ManagerTTL", ttl );
      if( inactiveTime >= ttl ) return true;
      return false;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  // Multiplex
  //----------------------------------------------------------------------------
  uint16_t XRootDTransport::Multiplex( Message *msg, AnyObject &channelData )
  {
    return 0;
  }

  //----------------------------------------------------------------------------
  // Marshall
  //----------------------------------------------------------------------------
  Status XRootDTransport::Marshall( Message *msg )
  {
    ClientRequest *req = (ClientRequest*)msg->GetBuffer();
    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // kXR_protocol
      //------------------------------------------------------------------------
      case kXR_protocol:
        req->protocol.clientpv  = htonl( req->protocol.clientpv );
        req->protocol.dlen      = htonl( req->protocol.dlen );
        break;

      //------------------------------------------------------------------------
      // kXR_login
      //------------------------------------------------------------------------
      case kXR_login:
        req->login.pid  = htonl( req->login.pid );
        req->login.dlen = htonl( req->login.dlen );
        break;
    };

    req->header.requestid = htons( req->header.requestid );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Unmarshall the body of the incomming message
  //----------------------------------------------------------------------------
  Status XRootDTransport::UnMarshallBody( Message *msg, XRequestTypes reqType )
  {
    ServerResponse *m = (ServerResponse *)msg->GetBuffer();

    //--------------------------------------------------------------------------
    // Unmarshall the body of a successful response
    //--------------------------------------------------------------------------
    if( m->hdr.status == kXR_ok )
    {
      switch( reqType )
      {
        //----------------------------------------------------------------------
        // kXR_protocol
        //----------------------------------------------------------------------
        case kXR_protocol:
          if( m->hdr.dlen != 8 )
            return Status( stError, errInvalidMessage );
          m->body.protocol.pval  = ntohl( m->body.protocol.pval );
          m->body.protocol.flags = ntohl( m->body.protocol.flags );
          break;
      }
    }
    //--------------------------------------------------------------------------
    // Unmarshall body of an error response
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_error )
      m->body.error.errnum = ntohl( m->body.error.errnum );

    return Status();
  }

  //------------------------------------------------------------------------
  // Unmarshall the header of the incomming message
  //------------------------------------------------------------------------
  void XRootDTransport::UnMarshallHeader( Message *msg )
  {
    ServerResponseHeader *header = (ServerResponseHeader *)msg->GetBuffer();
    header->status = ntohs( header->status );
    header->dlen   = ntohl( header->dlen );
  }

  //----------------------------------------------------------------------------
  // Log server error response
  //----------------------------------------------------------------------------
  void XRootDTransport::LogErrorResponse( const Message &msg )
  {
    Log *log = Utils::GetDefaultLog();
    ServerResponseBody_Error *err = (ServerResponseBody_Error *)msg.GetBuffer(8);
    log->Error( XRootDTransportMsg, "Server responded with an error [%d]: %s",
                                    err->errnum, err->errmsg );
  }

  //----------------------------------------------------------------------------
  // Generate the message to be sent as an initial handshake
  //----------------------------------------------------------------------------
  Message *XRootDTransport::GenerateInitialHS( HandShakeData     *hsData,
                                               XRootDChannelInfo *info )
  {
    Log *log = Utils::GetDefaultLog();
    log->Debug( XRootDTransportMsg, "[%s #%d] Sending out the initial "
                                    "hand shake",
                                     hsData->url->GetHostId().c_str(),
                                     hsData->streamId );

    Message *msg = new Message();

    msg->Allocate( 20+sizeof(ClientProtocolRequest) );
    msg->Zero();

    ClientInitHandShake   *init  = (ClientInitHandShake *)msg->GetBuffer();
    ClientProtocolRequest *proto = (ClientProtocolRequest *)msg->GetBuffer(20);
    init->fourth = htonl(4);
    init->fifth  = htonl(2012);

    proto->requestid = htons(kXR_protocol);
    proto->clientpv  = htonl(kXR_PROTOCOLVERSION);
    return msg;
  }

  //----------------------------------------------------------------------------
  // Process the server initial handshake response
  //----------------------------------------------------------------------------
  Status XRootDTransport::ProcessServerHS( HandShakeData     *hsData,
                                           XRootDChannelInfo *info )
  {
    Log *log = Utils::GetDefaultLog();

    Message *msg = hsData->in;
    ServerResponseHeader *respHdr = (ServerResponseHeader *)msg->GetBuffer();
    ServerInitHandShake  *hs      = (ServerInitHandShake *)msg->GetBuffer(4);

    if( respHdr->status != kXR_ok )
    {
      log->Error( XRootDTransportMsg,
                  "[%s #%d] Invalid hand shake response",
                  hsData->url->GetHostId().c_str(), hsData->streamId );

      return Status( stFatal, errHandShakeFailed );
    }

    info->protocolVersion = ntohl(hs->protover);
    info->serverFlags     = ntohl(hs->msgval) == kXR_DataServer ?
                            kXR_isServer:
                            kXR_isManager;

    log->Debug( XRootDTransportMsg,
                "[%s #%d] Got the server hand shake response (%s, protocol "
                "version %x)",
                hsData->url->GetHostId().c_str(),
                hsData->streamId,
                ServerFlagsToStr( info->serverFlags ).c_str(),
                info->protocolVersion );

    return Status( stOK, suContinue );
  }

  //----------------------------------------------------------------------------
  // Process the protocol response
  //----------------------------------------------------------------------------
  Status XRootDTransport::ProcessProtocolResp( HandShakeData     *hsData,
                                               XRootDChannelInfo *info )
  {
    Log *log = Utils::GetDefaultLog();

    if( !UnMarshallBody( hsData->in, kXR_protocol ).IsOK() )
      return Status( stFatal, errHandShakeFailed );

    ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();


    if( rsp->hdr.status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s #%d] kXR_protocol request failed",
                                      hsData->url->GetHostId().c_str(),
                                      hsData->streamId );

      return Status( stFatal, errHandShakeFailed );
    }

    if( rsp->body.protocol.pval >= 0x297 )
      info->serverFlags = rsp->body.protocol.flags;

    log->Debug( XRootDTransportMsg,
                "[%s #%d] kXR_protocol successful (%s, protocol version %x)",
                hsData->url->GetHostId().c_str(), hsData->streamId,
                ServerFlagsToStr( info->serverFlags ).c_str(),
                info->protocolVersion );
        
    return Status( stOK, suContinue );
  }

  //----------------------------------------------------------------------------
  // Generate the login message
  //----------------------------------------------------------------------------
  Message *XRootDTransport::GenerateLogIn( HandShakeData *hsData,
                                           XRootDChannelInfo *info )
  {
    Log *log = Utils::GetDefaultLog();

    Message *msg = new Message( sizeof( ClientLoginRequest ) );
    ClientLoginRequest *loginReq = (ClientLoginRequest *)msg->GetBuffer();

    loginReq->requestid = kXR_login;
    loginReq->pid       = ::getpid();
    loginReq->capver[0] = kXR_asyncap | kXR_ver003;
    loginReq->role[0]   = kXR_useruser;
    loginReq->dlen      = 0;

    if( hsData->url->GetUserName().length() )
    {
      ::strncpy( (char*)loginReq->username,
                 hsData->url->GetUserName().c_str(), 8 );

      log->Debug( XRootDTransportMsg, "[%s #%d] Sending out kXR_login "
                                      "request, username: %s",
                                      hsData->url->GetHostId().c_str(),
                                      hsData->streamId, loginReq->username );
    }
    else
    {
        log->Debug( XRootDTransportMsg, "[%s #%d] Sending out kXR_login "
                                        "request",
                                        hsData->url->GetHostId().c_str(),
                                        hsData->streamId );
    }
    Marshall( msg );
    return msg;
  }

  //----------------------------------------------------------------------------
  // Process the protocol response
  //----------------------------------------------------------------------------
  Status XRootDTransport::ProcessLogInResp( HandShakeData     *hsData,
                                            XRootDChannelInfo *info )
  {
    Log *log = Utils::GetDefaultLog();

    if( !UnMarshallBody( hsData->in, kXR_login ).IsOK() )
      return Status( stFatal, errLoginFailed );

    ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();

    if( rsp->hdr.status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s #d] Got invalid login response",
                                      hsData->url->GetHostId().c_str(),
                                      hsData->streamId );
      return Status( stFatal, errLoginFailed );
    }

    memcpy( info->sessionId, rsp->body.login.sessid, 16 );

    log->Debug( XRootDTransportMsg, "[%s #%d] Logged in",
                                    hsData->url->GetHostId().c_str(),
                                    hsData->streamId );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Get a string representation of the server flags
  //----------------------------------------------------------------------------
  std::string XRootDTransport::ServerFlagsToStr( uint32_t flags )
  {
    std::string repr = "type: ";
    if( flags & kXR_isManager )
      repr += "manager ";

    else if( flags & kXR_isServer )
      repr += "server ";

    repr += "[";

    if( flags & kXR_attrMeta )
      repr += "meta ";

    else if( flags & kXR_attrProxy )
      repr += "proxy ";

    else if( flags & kXR_attrSuper )
      repr += "super ";

    else
      repr += " ";

    repr.erase( repr.length()-1, 1 );

    repr += "]";
    return repr;
  }
}
