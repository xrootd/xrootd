//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucErrInfo.hh"

#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Information holder for XRootDStreams
  //----------------------------------------------------------------------------
  struct XRootDStreamInfo
  {
    //--------------------------------------------------------------------------
    // Define the stream status for the link negotiation purposes
    //--------------------------------------------------------------------------
    enum StreamStatus
    {
      Disconnected,
      Broken,
      HandShakeSent,
      HandShakeReceived,
      LoginSent,
      AuthSent,
      Connected
    };

    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------
    XRootDStreamInfo(): status( Disconnected )
    {
    }

    StreamStatus status;
  };

  //----------------------------------------------------------------------------
  //! Information holder for xrootd channels
  //----------------------------------------------------------------------------
  struct XRootDChannelInfo
  {
    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------
    XRootDChannelInfo():
      serverFlags(0),
      protocolVersion(0),
      sidManager(0),
      authBuffer(0),
      authProtocol(0),
      authParams(0),
      authEnv(0)
    {
      sidManager = new SIDManager();
      memset( sessionId, 0, 16 );
    }

    //--------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------
    ~XRootDChannelInfo()
    {
      delete    sidManager;
      delete [] authBuffer;
    }

    typedef std::vector<XRootDStreamInfo> StreamInfoVector;

    //--------------------------------------------------------------------------
    // Data
    //--------------------------------------------------------------------------
    uint32_t          serverFlags;
    uint32_t          protocolVersion;
    uint8_t           sessionId[16];
    SIDManager       *sidManager;
    char             *authBuffer;
    XrdSecProtocol   *authProtocol;
    XrdSecParameters *authParams;
    XrdOucEnv        *authEnv;
    StreamInfoVector  stream;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  XRootDTransport::XRootDTransport():
    pSecLibHandle(0),
    pAuthHandler(0)
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  XRootDTransport::~XRootDTransport()
  {
    if( pSecLibHandle )
      dlclose( pSecLibHandle );
  }

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

    Log *log = DefaultEnv::GetLog();
    ServerResponse *rsp = (ServerResponse *)message->GetBuffer();
    log->Dump( XRootDTransportMsg, "%s Read message 0x%x, size: %d, stream "
                                   "[%d, %d]",
                                   socket->GetName().c_str(), message,
                                   message->GetSize(),
                                  rsp->hdr.streamid[0], rsp->hdr.streamid[1] );

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
  void XRootDTransport::FinalizeChannel( AnyObject & )
  {
  }

  //----------------------------------------------------------------------------
  // HandShake
  //----------------------------------------------------------------------------
  Status XRootDTransport::HandShake( HandShakeData *handShakeData,
                                     AnyObject     &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    if( info->stream.size() <= handShakeData->streamId )
      info->stream.resize( handShakeData->streamId+1 );
    XRootDStreamInfo &sInfo = info->stream[handShakeData->streamId];

    //--------------------------------------------------------------------------
    // First step - we need to create and initial handshake and send it out
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::Disconnected ||
        sInfo.status == XRootDStreamInfo::Broken )
    {
      handShakeData->out = GenerateInitialHS( handShakeData, info );
      sInfo.status = XRootDStreamInfo::HandShakeSent;
      return Status( stOK, suContinue );
    }

    //--------------------------------------------------------------------------
    // Second step - we got the reply message to the initial handshake
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::HandShakeSent )
    {
      Status st = ProcessServerHS( handShakeData, info );
      if( st.IsOK() )
        sInfo.status = XRootDStreamInfo::HandShakeReceived;
      else
        sInfo.status = XRootDStreamInfo::Broken;
      return st;
    }

    //--------------------------------------------------------------------------
    // Third step - we got the response to the protocol request, we need
    // to process it and send out a login request
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::HandShakeReceived )
    {
      Status st =  ProcessProtocolResp( handShakeData, info );

      if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
        return st;
      }

      handShakeData->out = GenerateLogIn( handShakeData, info );
      sInfo.status = XRootDStreamInfo::LoginSent;
      return Status( stOK, suContinue );
    }

    //--------------------------------------------------------------------------
    // Fourth step - handle the log in response and proceed with the
    // authentication if required by the server
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::LoginSent )
    {
      Status st = ProcessLogInResp( handShakeData, info );

      if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
        return st;
      }

      if( st.IsOK() && st.code == suDone )
      {
        sInfo.status = XRootDStreamInfo::Connected;
        return st;
      }

      st = DoAuthentication( handShakeData, info );
      if( !st.IsOK() )
        sInfo.status = XRootDStreamInfo::Broken;
      else
        sInfo.status = XRootDStreamInfo::AuthSent;
      return st;
    }

    //--------------------------------------------------------------------------
    // Fifth step and later - proceed with the authentication
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::AuthSent )
    {
      Status st = DoAuthentication( handShakeData, info );

      if( !st.IsOK() )
        sInfo.status = XRootDStreamInfo::Broken;

      if( st.IsOK() && st.code == suDone )
        sInfo.status = XRootDStreamInfo::Connected;

      return st;
    }

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
  uint16_t XRootDTransport::Multiplex( Message *, AnyObject & )
  {
    return 0;
  }

  //----------------------------------------------------------------------------
  // Marshall
  //----------------------------------------------------------------------------
  Status XRootDTransport::MarshallRequest( Message *msg )
  {
    ClientRequest *req = (ClientRequest*)msg->GetBuffer();
    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // kXR_protocol
      //------------------------------------------------------------------------
      case kXR_protocol:
        req->protocol.clientpv = htonl( req->protocol.clientpv );
        break;

      //------------------------------------------------------------------------
      // kXR_login
      //------------------------------------------------------------------------
      case kXR_login:
        req->login.pid = htonl( req->login.pid );
        break;

      //------------------------------------------------------------------------
      // kXR_locate
      //------------------------------------------------------------------------
      case kXR_locate:
        req->locate.options = htons( req->locate.options );
        break;

      //------------------------------------------------------------------------
      // kXR_query
      //------------------------------------------------------------------------
      case kXR_query:
        req->query.infotype = htons( req->query.infotype );
        break;

      //------------------------------------------------------------------------
      // kXR_truncate
      //------------------------------------------------------------------------
      case kXR_truncate:
        req->truncate.offset = htonll( req->truncate.offset );
        break;

      //------------------------------------------------------------------------
      // kXR_mkdir
      //------------------------------------------------------------------------
      case kXR_mkdir:
        req->mkdir.mode = htons( req->mkdir.mode );
        break;

      //------------------------------------------------------------------------
      // kXR_chmod
      //------------------------------------------------------------------------
      case kXR_chmod:
        req->chmod.mode = htons( req->chmod.mode );
        break;

      //------------------------------------------------------------------------
      // kXR_open
      //------------------------------------------------------------------------
      case kXR_open:
        req->open.mode    = htons( req->open.mode );
        req->open.options = htons( req->open.options );
        break;
    };

    req->header.requestid = htons( req->header.requestid );
    req->header.dlen      = htonl( req->header.dlen );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Unmarshal the request - sometimes the requests need to be rewritten,
  // so we need to unmarshall them
  //----------------------------------------------------------------------------
  Status XRootDTransport::UnMarshallRequest( Message *msg )
  {
    // We rely on the marshaling process to be symetric!
    ClientRequest *req = (ClientRequest*)msg->GetBuffer();
    req->header.requestid = htons( req->header.requestid );
    Status st = MarshallRequest( msg );
    req->header.requestid = htons( req->header.requestid );
    return st;
  }

  //----------------------------------------------------------------------------
  // Unmarshall the body of the incomming message
  //----------------------------------------------------------------------------
  Status XRootDTransport::UnMarshallBody( Message *msg, uint16_t reqType )
  {
    ServerResponse *m = (ServerResponse *)msg->GetBuffer();

    //--------------------------------------------------------------------------
    // kXR_ok
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
    // kXR_error
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_error )
      m->body.error.errnum = ntohl( m->body.error.errnum );

    //--------------------------------------------------------------------------
    // kXR_wait
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_wait )
      m->body.wait.seconds = htonl( m->body.wait.seconds );

    //--------------------------------------------------------------------------
    // kXR_redirect
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_redirect )
      m->body.redirect.port = htonl( m->body.redirect.port );

    //--------------------------------------------------------------------------
    // kXR_waitresp
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_waitresp )
      m->body.waitresp.seconds = htonl( m->body.waitresp.seconds );

    //--------------------------------------------------------------------------
    // kXR_attn
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_attn )
      m->body.attn.actnum = htonl( m->body.attn.actnum );

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
    Log *log = DefaultEnv::GetLog();
    ServerResponseBody_Error *err = (ServerResponseBody_Error *)msg.GetBuffer(8);
    log->Error( XRootDTransportMsg, "Server responded with an error [%d]: %s",
                                    err->errnum, err->errmsg );
  }

  //----------------------------------------------------------------------------
  // The stream has been disconnected, do the cleanups
  //----------------------------------------------------------------------------
  void XRootDTransport::Disconnect( AnyObject &channelData, uint16_t streamId )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    if( !info->stream.empty() )
    {
      XRootDStreamInfo &sInfo = info->stream[streamId];
      sInfo.status = XRootDStreamInfo::Disconnected;
    }
  }

  //------------------------------------------------------------------------
  // Query the channel
  //------------------------------------------------------------------------
  Status XRootDTransport::Query( uint16_t   query,
                                 AnyObject &result,
                                 AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );

    switch( query )
    {
      //------------------------------------------------------------------------
      // Protocol name
      //------------------------------------------------------------------------
      case TransportQuery::Name:
        result.Set( (const char*)"XRootD", false );
        return Status();

      //------------------------------------------------------------------------
      // SID Manager object
      //------------------------------------------------------------------------
      case XRootDQuery::SIDManager:
        result.Set( info->sidManager, false );
        return Status();
    };
    return Status( stError, errQueryNotSupported );
  }

  //----------------------------------------------------------------------------
  // Generate the message to be sent as an initial handshake
  //----------------------------------------------------------------------------
  Message *XRootDTransport::GenerateInitialHS( HandShakeData     *hsData,
                                               XRootDChannelInfo * )
  {
    Log *log = DefaultEnv::GetLog();
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
    Log *log = DefaultEnv::GetLog();

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
    Log *log = DefaultEnv::GetLog();

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
                                           XRootDChannelInfo * )
  {
    Log *log = DefaultEnv::GetLog();

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
    MarshallRequest( msg );
    return msg;
  }

  //----------------------------------------------------------------------------
  // Process the protocol response
  //----------------------------------------------------------------------------
  Status XRootDTransport::ProcessLogInResp( HandShakeData     *hsData,
                                            XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

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

    log->Debug( XRootDTransportMsg, "[%s #%d] Logged in",
                                    hsData->url->GetHostId().c_str(),
                                    hsData->streamId );

    memcpy( info->sessionId, rsp->body.login.sessid, 16 );

    //--------------------------------------------------------------------------
    // We have an authentication info to process
    //--------------------------------------------------------------------------
    if( rsp->hdr.dlen > 16 )
    {
      size_t len = rsp->hdr.dlen-16;
      info->authBuffer = new char[len+1];
      info->authBuffer[len] = 0;
      memcpy( info->authBuffer, rsp->body.login.sec, len );
      log->Debug( XRootDTransportMsg, "[%s #%d] Authentication is required: %s",
                                      hsData->url->GetHostId().c_str(),
                                      hsData->streamId,
                                      info->authBuffer );

      return Status( stOK, suContinue );
    }

    return Status();
  }

  //----------------------------------------------------------------------------
  // Do the authentication
  //----------------------------------------------------------------------------
  Status XRootDTransport::DoAuthentication( HandShakeData     *hsData,
                                            XRootDChannelInfo *info )
  {
    //--------------------------------------------------------------------------
    // Prepare
    //--------------------------------------------------------------------------
    Log               *log   = DefaultEnv::GetLog();
    XRootDStreamInfo  &sInfo = info->stream[hsData->streamId];
    XrdSecCredentials *credentials = 0;
    std::string        protocolName;

    //--------------------------------------------------------------------------
    // We're doing this for the first time
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::LoginSent )
    {
      log->Debug( XRootDTransportMsg, "[%s #%d] Sending authentication data",
                                      hsData->url->GetHostId().c_str(),
                                      hsData->streamId );

      //------------------------------------------------------------------------
      // Initialize some structs
      //------------------------------------------------------------------------
      info->authEnv = new XrdOucEnv();
      info->authEnv->Put( "sockname", hsData->clientName.c_str() );

      size_t authBuffLen = strlen( info->authBuffer );
      char *pars = (char *)malloc( authBuffLen );
      memcpy( pars, info->authBuffer, authBuffLen );
      info->authParams = new XrdSecParameters( pars, authBuffLen );
      sInfo.status = XRootDStreamInfo::AuthSent;

      //------------------------------------------------------------------------
      // Find a protocol that gives us valid credentials
      //------------------------------------------------------------------------
      Status st = GetCredentials( credentials, hsData, info );
      if( !st.IsOK() )
      {
        CleanUpAuthentication( info );
        return st;
      }
      protocolName = info->authProtocol->Entity.prot;
    }

    //--------------------------------------------------------------------------
    // We've been here already
    //--------------------------------------------------------------------------
    else
    {
      ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();
      protocolName = info->authProtocol->Entity.prot;

      //------------------------------------------------------------------------
      // We're required to send out more authentication data
      //------------------------------------------------------------------------
      if( rsp->hdr.status == kXR_authmore )
      {
        log->Debug( XRootDTransportMsg, "[%s #%d] Sending more authentication "
                                        "data for %s",
                                        hsData->url->GetHostId().c_str(),
                                        hsData->streamId,
                                        protocolName.c_str() );

        uint32_t          len      = rsp->hdr.dlen;
        char             *secTokenData = (char*)malloc( len );
        memcpy( secTokenData, rsp->body.authmore.data, len );
        XrdSecParameters *secToken     = new XrdSecParameters( secTokenData, len );
        XrdOucErrInfo     ei( "", info->authEnv);
        credentials = info->authProtocol->getCredentials( secToken, &ei );
        delete secToken;

        //----------------------------------------------------------------------
        // The protocol handler refuses to give us the data
        //----------------------------------------------------------------------
        if( !credentials )
        {
          log->Debug( XRootDTransportMsg, "[%s #%d] Auth protocol handler for "
                                          "%s refuses to give us more "
                                          "credentials %s",
                                          hsData->url->GetHostId().c_str(),
                                          hsData->streamId,
                                          protocolName.c_str(),
                                          ei.getErrText() );
          CleanUpAuthentication( info );
          return Status( stFatal, errAuthFailed );
        }
      }

      //------------------------------------------------------------------------
      // We have either succeeded or failed, in either case we need to clean up
      //------------------------------------------------------------------------
      else
      {
        CleanUpAuthentication( info );

        //----------------------------------------------------------------------
        // Success
        //----------------------------------------------------------------------
        if( rsp->hdr.status == kXR_ok )
        {
          log->Debug( XRootDTransportMsg, "[%s #%d] Authenticated with %s.",
                                          hsData->url->GetHostId().c_str(),
                                          hsData->streamId,
                                          protocolName.c_str() );
          return Status();
        }

        //----------------------------------------------------------------------
        // Failure
        //----------------------------------------------------------------------
        else if( rsp->hdr.status == kXR_error )
        {
          log->Error( XRootDTransportMsg, "[%s #%d] Authentication with %s "
                                          "failed: %s",
                                          hsData->url->GetHostId().c_str(),
                                          hsData->streamId,
                                          protocolName.c_str(),
                                          rsp->body.error.errmsg );

          return Status( stError, errAuthFailed );
        }

        //----------------------------------------------------------------------
        // God knows what
        //----------------------------------------------------------------------
        log->Error( XRootDTransportMsg, "[%s #%d] Authentication with %s "
                                        "failed: unexpected answer",
                                        hsData->url->GetHostId().c_str(),
                                        hsData->streamId,
                                        protocolName.c_str() );
        return Status( stError, errAuthFailed );
      }
    }

    //--------------------------------------------------------------------------
    // Generate the client request
    //--------------------------------------------------------------------------
    Message *msg = new Message( sizeof(ClientAuthRequest)+credentials->size );
    msg->Zero();
    ClientRequest *req       = (ClientRequest*)msg->GetBuffer();
    char          *reqBuffer = msg->GetBuffer(sizeof(ClientAuthRequest));

    req->header.requestid = kXR_auth;
    req->auth.dlen        = credentials->size;
    memcpy( req->auth.credtype, protocolName.c_str(),
            protocolName.length() > 4 ? 4 : protocolName.length() );

    memcpy( reqBuffer, credentials->buffer, credentials->size );
    hsData->out = msg;
    MarshallRequest( msg );
    delete credentials;
    return Status( stOK, suContinue );
  }

  //------------------------------------------------------------------------
  // Get the initial credentials using one of the protocols
  //------------------------------------------------------------------------
  Status XRootDTransport::GetCredentials( XrdSecCredentials *&credentials,
                                          HandShakeData      *hsData,
                                          XRootDChannelInfo  *info )
  {
    //--------------------------------------------------------------------------
    // Set up the auth handler
    //--------------------------------------------------------------------------
    Log             *log   = DefaultEnv::GetLog();
    XrdOucErrInfo    ei( "", info->authEnv);
    XrdSecGetProt_t  authHandler = GetAuthHandler();
    if( !authHandler )
      return Status( stFatal, errAuthFailed );

    //--------------------------------------------------------------------------
    // Loop over the possible protocols to find one that gives us valid
    // credentials
    //--------------------------------------------------------------------------
    while(1)
    {
      //------------------------------------------------------------------------
      // Get the protocol
      //------------------------------------------------------------------------
      info->authProtocol = (*authHandler)( hsData->url->GetHostName().c_str(),
                                           *((sockaddr*)hsData->serverAddr),
                                           *info->authParams,
                                           0 );
      if( !info->authProtocol )
      {
        log->Error( XRootDTransportMsg, "[%s #%d] No protocols left to try",
                                        hsData->url->GetHostId().c_str(),
                                        hsData->streamId );
        return Status( stFatal, errAuthFailed );
      }

      std::string protocolName = info->authProtocol->Entity.prot;
      log->Debug( XRootDTransportMsg, "[%s #%d] Trying to authenticate using %s",
                                      hsData->url->GetHostId().c_str(),
                                      hsData->streamId,
                                      protocolName.c_str() );

      //------------------------------------------------------------------------
      // Get the credentials from the current protocol
      //------------------------------------------------------------------------
      credentials = info->authProtocol->getCredentials( 0, &ei );
      if( !credentials )
      {
        log->Debug( XRootDTransportMsg, "[%s #%d] Cannot get credentials for "
                                        "protocol %s: %s",
                                        hsData->url->GetHostId().c_str(),
                                        hsData->streamId,
                                        protocolName.c_str(),
                                        ei.getErrText() );
        info->authProtocol->Delete();
        continue;
      }
      return Status( stOK, suContinue );
    }
  }

  //------------------------------------------------------------------------
  // Clean up the data structures created for the authentication process
  //------------------------------------------------------------------------
  Status XRootDTransport::CleanUpAuthentication( XRootDChannelInfo *info )
  {
    if( info->authProtocol )
      info->authProtocol->Delete();
    delete info->authParams;
    delete info->authEnv;
    info->authProtocol = 0;
    info->authParams   = 0;
    info->authEnv      = 0;
    return Status();
  }

  //----------------------------------------------------------------------------
  // Get the authentication function handle
  //----------------------------------------------------------------------------
  XRootDTransport::XrdSecGetProt_t XRootDTransport::GetAuthHandler()
  {
    Log *log = DefaultEnv::GetLog();

    if( pAuthHandler )
      return pAuthHandler;

    //--------------------------------------------------------------------------
    // dlopen the library
    //--------------------------------------------------------------------------
    std::string libName = "libXrdSec"; libName += LT_MODULE_EXT;
    pSecLibHandle = ::dlopen( libName.c_str(), RTLD_NOW );
    if( !pSecLibHandle )
    {
      log->Error( XRootDTransportMsg, "Unable to load the authentication "
                                      "library %s: %s",
                                      libName.c_str(), ::dlerror() );
      return 0;
    }

    //--------------------------------------------------------------------------
    // Get the authentication function handle
    //--------------------------------------------------------------------------
    pAuthHandler = (XrdSecGetProt_t)dlsym( pSecLibHandle, "XrdSecGetProtocol" );
    if( !pAuthHandler )
    {
      log->Error( XRootDTransportMsg, "Unable to get the XrdSecGetProtocol "
                                      "symbol from library %s: %s",
                                      libName.c_str(), ::dlerror() );
      ::dlclose( pSecLibHandle );
      pSecLibHandle = 0;
      return 0;
    }
    return pAuthHandler;
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
