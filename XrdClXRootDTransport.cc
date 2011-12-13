//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
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
          return Status( stError, errRetry );

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
        return Status( stError, errRetry );

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
  void XRootDTransport::InitializeChannel( void *&channelData )
  {
    channelData = new XRootDChannelInfo();
  }

  //----------------------------------------------------------------------------
  // Finalize channel
  //----------------------------------------------------------------------------
  void XRootDTransport::FinalizeChannel( void *&channelData )
  {
    delete (XRootDChannelInfo*)channelData;
  }

  //----------------------------------------------------------------------------
  // HandShake
  //----------------------------------------------------------------------------
  Status XRootDTransport::HandShake( Socket    *socket,
                                     const URL &url,
                                     uint16_t   streamNumber,
                                     void      *channelData )
  {
    Log *log = Utils::GetDefaultLog();
    log->Debug( XRootDTransportMsg, "[%s #%d] Attempting handshake",
                                    url.GetHostId().c_str(), streamNumber );

    XRootDChannelInfo *info = (XRootDChannelInfo *)channelData;

    //--------------------------------------------------------------------------
    // Send the initial handshake
    //--------------------------------------------------------------------------
    Status sc = InitialHS( socket, url, streamNumber, info );
    if( sc.status != stOK )
      return sc;

    //--------------------------------------------------------------------------
    // Log in
    //--------------------------------------------------------------------------
    return LogIn( socket, url, streamNumber, info );
  }

  //----------------------------------------------------------------------------
  // Disconnect
  //----------------------------------------------------------------------------
  Status XRootDTransport::Disconnect( Socket    *socket,
                                      uint16_t   streamNumber,
                                      void     *channelData )
  {
    return Status();
  }

  //----------------------------------------------------------------------------
  // Multiplex
  //----------------------------------------------------------------------------
  uint16_t XRootDTransport::Multiplex( Message *msg, void *channelData )
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
  // Perform the initial handshake
  //----------------------------------------------------------------------------
  Status XRootDTransport::InitialHS( Socket            *socket,
                                     const URL         &url,
                                     uint16_t           streamNumber,
                                     XRootDChannelInfo *info )
  {
    //--------------------------------------------------------------------------
    // Configure
    //--------------------------------------------------------------------------
    Env *env = DefaultEnv::GetEnv();
    Log *log = Utils::GetDefaultLog();

    int requestTimeout = DefaultRequestTimeout;
    env->GetInt( "RequestTimeout", requestTimeout );

    //--------------------------------------------------------------------------
    // Build the initial handshake message and piggy back the kXR_protocol
    // request at the end
    //--------------------------------------------------------------------------
    Message req;
    req.Allocate( 20+sizeof(ClientProtocolRequest) );
    req.Zero();

    ClientInitHandShake   *init  = (ClientInitHandShake *)req.GetBuffer();
    ClientProtocolRequest *proto = (ClientProtocolRequest *)req.GetBuffer(20);

    init->fourth = htonl(4);
    init->fifth  = htonl(2012);

    proto->requestid = htons(kXR_protocol);
    proto->clientpv  = htonl(kXR_PROTOCOLVERSION);

    uint32_t bytesProcessed;

    //--------------------------------------------------------------------------
    // Write the data
    //--------------------------------------------------------------------------
    log->Debug( XRootDTransportMsg, "[%s #%d] Sending the hand shake",
                                    url.GetHostId().c_str(), streamNumber );

    socket->WriteRaw( req.GetBuffer(), req.GetSize(),
                      requestTimeout, bytesProcessed );

    if( bytesProcessed != req.GetSize() )
    {
      log->Error( XRootDTransportMsg, "[%s #%d] Unable to send the initial "
                                      "hand shake",
                                      url.GetHostId().c_str(), streamNumber );
      return Status( stFatal, errHandShakeFailed );
    }

    //--------------------------------------------------------------------------
    // Read the handshake
    //--------------------------------------------------------------------------
    Message resp(16);

    log->Dump( XRootDTransportMsg, "[%s #%d] Waiting %d seconds for 16 bytes",
                                   url.GetHostId().c_str(), streamNumber,
                                   requestTimeout );

    socket->ReadRaw( resp.GetBuffer(), 16, requestTimeout, bytesProcessed );

    if( bytesProcessed != resp.GetSize() )
    {
      log->Error( XRootDTransportMsg, "[%s #%d] Unable to get the initial "
                                      "hand shake",
                                      url.GetHostId().c_str(), streamNumber );
      return Status( stFatal, errHandShakeFailed );
    }

    //--------------------------------------------------------------------------
    // Process the handshake
    //--------------------------------------------------------------------------
    ServerResponseHeader *respHdr = (ServerResponseHeader *)resp.GetBuffer();
    ServerInitHandShake  *hs      = (ServerInitHandShake *)resp.GetBuffer(4);

    if( respHdr->status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s #%d] Invalid hand shake response "
                                      "came",
                                      url.GetHostId().c_str(), streamNumber );
      return Status( stFatal, errHandShakeFailed );
    }

    info->protocolVersion = ntohl(hs->protover);
    info->serverFlags     = ntohl(hs->msgval) == kXR_DataServer ?
                            kXR_isServer:
                            kXR_isManager;

    //--------------------------------------------------------------------------
    // Read and process the protocol response
    //--------------------------------------------------------------------------
    if( GetMessageBlock( &resp, socket, requestTimeout ).status != stOK )
      return Status( stFatal, errHandShakeFailed );

    if( UnMarshallBody( &resp, kXR_protocol ).status != stOK )
      return Status( stFatal, errHandShakeFailed );

    ServerResponse *rsp = (ServerResponse*)resp.GetBuffer();
    if( rsp->hdr.status == kXR_error )
    {
      LogErrorResponse( resp );
      return Status( stFatal, errHandShakeFailed );
    }
    else if( rsp->hdr.status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s #%d] Got invalid response to "
                                      "kXR_protocol",
                                      url.GetHostId().c_str(), streamNumber );
      return Status( stFatal, errHandShakeFailed );
    }

    if( rsp->body.protocol.pval >= 0x297 )
      info->serverFlags = rsp->body.protocol.flags;

    log->Debug( XRootDTransportMsg,
                "[%s #%d] Hand shake successful (%s, protocol version %x)",
                url.GetHostId().c_str(), streamNumber,
                ServerFlagsToStr( info->serverFlags ).c_str(),
                info->protocolVersion );
        
    return Status();
  }

  //----------------------------------------------------------------------------
  // Log in
  //----------------------------------------------------------------------------
  Status XRootDTransport::LogIn( Socket            *socket,
                                 const URL         &url,
                                 uint16_t           streamNumber,
                                 XRootDChannelInfo *info )
  {
    //--------------------------------------------------------------------------
    // Configure
    //--------------------------------------------------------------------------
    Env *env = DefaultEnv::GetEnv();
    Log *log = Utils::GetDefaultLog();

    int requestTimeout = DefaultRequestTimeout;
    env->GetInt( "RequestTimeout", requestTimeout );

    //--------------------------------------------------------------------------
    // Build the login request
    //--------------------------------------------------------------------------
    log->Debug( XRootDTransportMsg, "[%s #%d] Logging in",
                                    url.GetHostId().c_str(), streamNumber );

    Message m( sizeof( ClientLoginRequest ) );
    ClientLoginRequest *loginReq = (ClientLoginRequest *)m.GetBuffer();
    loginReq->requestid = kXR_login;
    loginReq->pid       = ::getpid();
    if( url.GetUserName().length() )
      ::strncpy( (char*)loginReq->username, url.GetUserName().c_str(), 8 );
    loginReq->capver[0] = kXR_asyncap | kXR_ver003;
    loginReq->role[0]   = kXR_useruser;
    loginReq->dlen      = 0;

    Marshall( &m );

    //--------------------------------------------------------------------------
    // Send the request
    //--------------------------------------------------------------------------
    uint32_t processedBytes;
    Status sc = socket->WriteRaw( m.GetBuffer(), m.GetSize(),
                                  requestTimeout, processedBytes );

    if( processedBytes != m.GetSize() )
    {
      log->Error( XRootDTransportMsg, "[%s #%d] Unable send the login request",
                                      url.GetHostId().c_str(), streamNumber );
      return Status( stFatal, errLoginFailed );
    }

    //--------------------------------------------------------------------------
    // Read the response
    //--------------------------------------------------------------------------
    if( GetMessageBlock( &m, socket, requestTimeout ).status != stOK )
      return Status( stFatal, errLoginFailed );

    if( UnMarshallBody( &m, kXR_login ).status != stOK )
      return Status( stFatal, errLoginFailed );

    ServerResponse *rsp = (ServerResponse*)m.GetBuffer();
    if( rsp->hdr.status == kXR_error )
    {
      LogErrorResponse( m );
      return Status( stFatal, errHandShakeFailed );
    }
    else if( rsp->hdr.status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s #d] Got invalid login response",
                                      url.GetHostId().c_str(), streamNumber );
      return Status( stFatal, errLoginFailed );
    }

    memcpy( info->sessionId, rsp->body.login.sessid, 16 );

    log->Debug( XRootDTransportMsg, "[%s #%d] Logged in",
                                    url.GetHostId().c_str(), streamNumber );

    return Status();
  }

  //----------------------------------------------------------------------------
  // Get a message - blocking
  //----------------------------------------------------------------------------
  Status XRootDTransport::GetMessageBlock( Message *message,
                                           Socket  *socket,
                                           uint16_t timeout )
  {
    Log *log = Utils::GetDefaultLog();

    //--------------------------------------------------------------------------
    // Read the header
    //--------------------------------------------------------------------------
    uint32_t readBytes;
    message->ReAllocate(8);

    log->Dump( XRootDTransportMsg, "%s Waiting %d seconds for a message "
                                   "header",
                                   socket->GetName().c_str(), timeout );

    time_t start = ::time(0);
    Status sc = socket->ReadRaw( message->GetBuffer(), 8, timeout, readBytes );

    if( readBytes != 8 )
    {
      log->Error( XRootDTransportMsg, "%s Error reading message header",
                                      socket->GetName().c_str() );
      return sc;
    }

    time_t elapsed = ::time(0)-start;
    if( elapsed >= timeout )
      return Status( stError, errSocketTimeout );

    //--------------------------------------------------------------------------
    // Read the body
    //--------------------------------------------------------------------------
    ServerResponseHeader *header = (ServerResponseHeader *)message->GetBuffer();
    UnMarshallHeader( message );
    message->ReAllocate( header->dlen+8 );
    header = (ServerResponseHeader *)message->GetBuffer();

    log->Dump( XRootDTransportMsg, "%s Waiting %d seconds for %d bytes of "
                                   "message body",
                                   socket->GetName().c_str(),
                                   timeout-elapsed, header->dlen );

    sc = socket->ReadRaw( message->GetBuffer(8), header->dlen,
                          timeout, readBytes );

    if( readBytes != header->dlen )
    {
      log->Error( XRootDTransportMsg, "%s Error reading message body",
                                      socket->GetName().c_str() );
      return sc;
    }

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
