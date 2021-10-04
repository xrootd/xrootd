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

#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClAsyncSocketHandler.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClOptimizers.hh"
#include "XrdSys/XrdSysE2T.hh"
#include <netinet/tcp.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  AsyncSocketHandler::AsyncSocketHandler( const URL        &url,
                                          Poller           *poller,
                                          TransportHandler *transport,
                                          AnyObject        *channelData,
                                          uint16_t          subStreamNum,
                                          Stream           *strm ):
    pPoller( poller ),
    pTransport( transport ),
    pChannelData( channelData ),
    pSubStreamNum( subStreamNum ),
    pStream( strm ),
    pStreamName( ToStreamName( strm, subStreamNum ) ),
    pSocket( new Socket() ),
    pIncoming( 0 ),
    pHSIncoming( 0 ),
    pOutgoing( 0 ),
    pSignature( 0 ),
    pHandShakeData( 0 ),
    pHandShakeDone( false ),
    pConnectionStarted( 0 ),
    pConnectionTimeout( 0 ),
    pHeaderDone( false ),
    pOutMsgDone( false ),
    pOutHandler( 0 ),
    pIncMsgSize( 0 ),
    pOutMsgSize( 0 ),
    pUrl( url ),
    pTlsHandShakeOngoing( false )
  {
    Env *env = DefaultEnv::GetEnv();

    int timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );
    pTimeoutResolution = timeoutResolution;

    pSocket->SetChannelID( pChannelData );
    pIncHandler = std::make_pair( (IncomingMsgHandler*)0, false );
    pLastActivity = time(0);
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  AsyncSocketHandler::~AsyncSocketHandler()
  {
    Close();
    delete pSocket;
    delete pSignature;
  }

  //----------------------------------------------------------------------------
  // Connect to given address
  //----------------------------------------------------------------------------
  XRootDStatus AsyncSocketHandler::Connect( time_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    pLastActivity = pConnectionStarted = ::time(0);
    pConnectionTimeout = timeout;

    //--------------------------------------------------------------------------
    // Initialize the socket
    //--------------------------------------------------------------------------
    XRootDStatus st = pSocket->Initialize( pSockAddr.Family() );
    if( !st.IsOK() )
    {
      log->Error( AsyncSockMsg, "[%s] Unable to initialize socket: %s",
                  pStreamName.c_str(), st.ToString().c_str() );
      st.status = stFatal;
      return st;
    }

    //--------------------------------------------------------------------------
    // Set the keep-alive up
    //--------------------------------------------------------------------------
    Env *env = DefaultEnv::GetEnv();

    int keepAlive = DefaultTCPKeepAlive;
    env->GetInt( "TCPKeepAlive", keepAlive );
    if( keepAlive )
    {
      int          param = 1;
      XRootDStatus st    = pSocket->SetSockOpt( SOL_SOCKET, SO_KEEPALIVE, &param,
                                          sizeof(param) );
      if( !st.IsOK() )
        log->Error( AsyncSockMsg, "[%s] Unable to turn on keepalive: %s",
                    st.ToString().c_str() );

#if ( defined(__linux__) || defined(__GNU__) ) && defined( TCP_KEEPIDLE ) && \
    defined( TCP_KEEPINTVL ) && defined( TCP_KEEPCNT )

      param = DefaultTCPKeepAliveTime;
      env->GetInt( "TCPKeepAliveTime", param );
      st = pSocket->SetSockOpt(SOL_TCP, TCP_KEEPIDLE, &param, sizeof(param));
      if( !st.IsOK() )
        log->Error( AsyncSockMsg, "[%s] Unable to set keepalive time: %s",
                    st.ToString().c_str() );

      param = DefaultTCPKeepAliveInterval;
      env->GetInt( "TCPKeepAliveInterval", param );
      st = pSocket->SetSockOpt(SOL_TCP, TCP_KEEPINTVL, &param, sizeof(param));
      if( !st.IsOK() )
        log->Error( AsyncSockMsg, "[%s] Unable to set keepalive interval: %s",
                    st.ToString().c_str() );

      param = DefaultTCPKeepAliveProbes;
      env->GetInt( "TCPKeepAliveProbes", param );
      st = pSocket->SetSockOpt(SOL_TCP, TCP_KEEPCNT, &param, sizeof(param));
      if( !st.IsOK() )
        log->Error( AsyncSockMsg, "[%s] Unable to set keepalive probes: %s",
                    st.ToString().c_str() );
#endif
    }

    pHandShakeDone = false;

    //--------------------------------------------------------------------------
    // Initiate async connection to the address
    //--------------------------------------------------------------------------
    char nameBuff[256];
    pSockAddr.Format( nameBuff, sizeof(nameBuff), XrdNetAddrInfo::fmtAdv6 );
    log->Debug( AsyncSockMsg, "[%s] Attempting connection to %s",
                pStreamName.c_str(), nameBuff );

    st = pSocket->ConnectToAddress( pSockAddr, 0 );
    if( !st.IsOK() )
    {
      log->Error( AsyncSockMsg, "[%s] Unable to initiate the connection: %s",
                  pStreamName.c_str(), st.ToString().c_str() );
      return st;
    }

    pSocket->SetStatus( Socket::Connecting );

    //--------------------------------------------------------------------------
    // We should get the ready to write event once we're really connected
    // so we need to listen to it
    //--------------------------------------------------------------------------
    if( !pPoller->AddSocket( pSocket, this ) )
    {
      XRootDStatus st( stFatal, errPollerError );
      pSocket->Close();
      return st;
    }

    if( !pPoller->EnableWriteNotification( pSocket, true, pTimeoutResolution ) )
    {
      XRootDStatus st( stFatal, errPollerError );
      pPoller->RemoveSocket( pSocket );
      pSocket->Close();
      return st;
    }

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Close the connection
  //----------------------------------------------------------------------------
  XRootDStatus AsyncSocketHandler::Close()
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( AsyncSockMsg, "[%s] Closing the socket", pStreamName.c_str() );

    pTransport->Disconnect( *pChannelData,
                            pSubStreamNum );

    pPoller->RemoveSocket( pSocket );
    pSocket->Close();

    if( !pIncHandler.second )
      delete pIncoming;

    pIncoming = 0;
    return XRootDStatus();
  }

  std::string AsyncSocketHandler::ToStreamName( Stream *stream, uint16_t strmnb )
  {
    std::ostringstream o;
    o << stream->GetURL()->GetHostId();
    o << "." << strmnb;
    return o.str();
  }

  //----------------------------------------------------------------------------
  // Handler a socket event
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::Event( uint8_t type, XrdCl::Socket */*socket*/ )
  {
//    //--------------------------------------------------------------------------
//    // First check if the socket itself wants to apply some mapping on the
//    // event. E.g. in case of TLS socket it might want to map read events to
//    // write events and vice-versa.
//    //--------------------------------------------------------------------------
    type = pSocket->MapEvent( type );

    //--------------------------------------------------------------------------
    // Read event
    //--------------------------------------------------------------------------
    if( type & ReadyToRead )
    {
      pLastActivity = time(0);
      if( unlikely( pTlsHandShakeOngoing ) )
        OnTLSHandShake();
      else if( likely( pHandShakeDone ) )
        OnRead();
      else
        OnReadWhileHandshaking();
    }

    //--------------------------------------------------------------------------
    // Read timeout
    //--------------------------------------------------------------------------
    else if( type & ReadTimeOut )
    {
      if( likely( pHandShakeDone ) )
        OnReadTimeout();
      else
        OnTimeoutWhileHandshaking();
    }

    //--------------------------------------------------------------------------
    // Write event
    //--------------------------------------------------------------------------
    if( type & ReadyToWrite )
    {
      pLastActivity = time(0);
      if( unlikely( pSocket->GetStatus() == Socket::Connecting ) )
        OnConnectionReturn();
      else if( unlikely( pTlsHandShakeOngoing ) )
        OnTLSHandShake();
      else if( likely( pHandShakeDone ) )
        OnWrite();
      else
        OnWriteWhileHandshaking();
    }

    //--------------------------------------------------------------------------
    // Write timeout
    //--------------------------------------------------------------------------
    else if( type & WriteTimeOut )
    {
      if( likely( pHandShakeDone ) )
        OnWriteTimeout();
      else
        OnTimeoutWhileHandshaking();
    }
  }

  //----------------------------------------------------------------------------
  // Connect returned
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnConnectionReturn()
  {
    //--------------------------------------------------------------------------
    // Check whether we were able to connect
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    log->Debug( AsyncSockMsg, "[%s] Async connection call returned",
                pStreamName.c_str() );

    int errorCode = 0;
    socklen_t optSize = sizeof( errorCode );
    XRootDStatus st = pSocket->GetSockOpt( SOL_SOCKET, SO_ERROR, &errorCode,
                                     &optSize );

    //--------------------------------------------------------------------------
    // This is an internal error really (either logic or system fault),
    // so we call it a day and don't retry
    //--------------------------------------------------------------------------
    if( !st.IsOK() )
    {
      log->Error( AsyncSockMsg, "[%s] Unable to get the status of the "
                  "connect operation: %s", pStreamName.c_str(),
                  XrdSysE2T( errno ) );
      pStream->OnConnectError( pSubStreamNum,
                               XRootDStatus( stFatal, errSocketOptError, errno ) );
      return;
    }

    //--------------------------------------------------------------------------
    // We were unable to connect
    //--------------------------------------------------------------------------
    if( errorCode )
    {
      log->Error( AsyncSockMsg, "[%s] Unable to connect: %s",
                  pStreamName.c_str(), XrdSysE2T( errorCode ) );
      pStream->OnConnectError( pSubStreamNum,
                               XRootDStatus( stError, errConnectionError ) );
      return;
    }
    pSocket->SetStatus( Socket::Connected );
    hswriter.reset( new MsgWriter( *pSocket, pStreamName ) );

    //--------------------------------------------------------------------------
    // Cork the socket
    //--------------------------------------------------------------------------
    st = pSocket->Cork();
    if( !st.IsOK() )
    {
      pStream->OnConnectError( pSubStreamNum, st );
      return;
    }

    //--------------------------------------------------------------------------
    // Initialize the handshake
    //--------------------------------------------------------------------------
    pHandShakeData = new HandShakeData( pStream->GetURL(),
                                        pSubStreamNum );
    pHandShakeData->serverAddr = pSocket->GetServerAddress();
    pHandShakeData->clientName = pSocket->GetSockName();
    pHandShakeData->streamName = pStreamName;

    st = pTransport->HandShake( pHandShakeData, *pChannelData );
    if( !st.IsOK() )
    {
      log->Error( AsyncSockMsg, "[%s] Connection negotiation failed",
                  pStreamName.c_str() );
      pStream->OnConnectError( pSubStreamNum, st );
      return;
    }

    if( st.code != suRetry )
      ++pHandShakeData->step;

    //--------------------------------------------------------------------------
    // Transport has given us something to send
    //--------------------------------------------------------------------------
    if( pHandShakeData->out )
    {
      hswriter->Reset( pHandShakeData->out );
      pHandShakeData->out = nullptr;
    }

    //--------------------------------------------------------------------------
    // Listen to what the server has to say
    //--------------------------------------------------------------------------
    if( !pPoller->EnableReadNotification( pSocket, true, pTimeoutResolution ) )
    {
      pStream->OnConnectError( pSubStreamNum,
                               XRootDStatus( stFatal, errPollerError ) );
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Got a write readiness event
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnWrite()
  {
    //--------------------------------------------------------------------------
    // Pick up a message if we're not in process of writing something
    //--------------------------------------------------------------------------
    if( !pOutgoing )
    {
      pOutMsgDone = false;
      std::pair<Message *, OutgoingMsgHandler *> toBeSent;
      toBeSent = pStream->OnReadyToWrite( pSubStreamNum );
      pOutgoing = toBeSent.first; pOutHandler = toBeSent.second;

      if( !pOutgoing )
        return;

      pOutgoing->SetCursor( 0 );
      pOutMsgSize = pOutgoing->GetSize();

      //------------------------------------------------------------------------
      // Secure the message if necessary
      //------------------------------------------------------------------------
      delete pSignature; pSignature = 0;
      XRootDStatus st = pTransport->GetSignature( pOutgoing, pSignature, *pChannelData );
      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }

      if( pSignature )
        pOutMsgSize += pSignature->GetSize();
    }

    //--------------------------------------------------------------------------
    // Write everything at once: signature, request and raw data
    //--------------------------------------------------------------------------
    XRootDStatus st = WriteMessageAndRaw( pOutgoing, pSignature );
    if( !st.IsOK() )
    {
      OnFault( st );
      return;
    }

    if( st.code == suRetry )
      return;

    Log *log = DefaultEnv::GetLog();
    log->Dump( AsyncSockMsg, "[%s] Successfully sent message: %s (0x%x).",
               pStreamName.c_str(), pOutgoing->GetDescription().c_str(),
               pOutgoing );

    pStream->OnMessageSent( pSubStreamNum, pOutgoing, pOutMsgSize );
    pOutgoing = 0;

    //--------------------------------------------------------------------------
    // Disable the respective substream if empty
    //--------------------------------------------------------------------------
    pStream->DisableIfEmpty( pSubStreamNum );
  }

  //----------------------------------------------------------------------------
  // Got a write readiness event while handshaking
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnWriteWhileHandshaking()
  {
    XRootDStatus st;
    if( !hswriter || !hswriter->HasMsg() )
    {
      if( !(st = DisableUplink()).IsOK() )
        OnFaultWhileHandshaking( st );
      return;
    }

    if( !( st = hswriter->Write() ).IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    if( st.code == suRetry ) return;
    hswriter->Reset();

    st = pSocket->Flash();
    if( !st.IsOK() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( AsyncSockMsg, "[%s] Unable to flash the socket: %s",
                  pStreamName.c_str(), XrdSysE2T( st.errNo ) );
      OnFaultWhileHandshaking( st );
    }

    if( !(st = DisableUplink()).IsOK() )
      OnFaultWhileHandshaking( st );
  }

  //----------------------------------------------------------------------------
  // Write the current message
  //----------------------------------------------------------------------------
  XRootDStatus AsyncSocketHandler::WriteCurrentMessage( Message *toWrite )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Try to write down the current message
    //--------------------------------------------------------------------------
    Message  *msg             = toWrite;
    size_t    leftToBeWritten = msg->GetSize()-msg->GetCursor();
    if( !leftToBeWritten ) return XRootDStatus();

    while( leftToBeWritten )
    {
      int bytesWritten = 0;
      XRootDStatus st = pSocket->Send( msg->GetBufferAtCursor(), leftToBeWritten, bytesWritten );

      if( !st.IsOK() )
      {
        toWrite->SetCursor( 0 );
        return st;
      }

      if( st.code == suRetry ) return st;

      msg->AdvanceCursor( bytesWritten );
      leftToBeWritten -= bytesWritten;
    }

    //--------------------------------------------------------------------------
    // We have written the message successfully
    //--------------------------------------------------------------------------
    log->Dump( AsyncSockMsg, "[%s] Wrote a message: %s (0x%x), %d bytes",
               pStreamName.c_str(), toWrite->GetDescription().c_str(),
               toWrite, toWrite->GetSize() );
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Write the message and its signature
  //----------------------------------------------------------------------------
  XRootDStatus AsyncSocketHandler::WriteMessageAndRaw( Message *toWrite, Message *&sign )
  {
    XRootDStatus st;
    Log *log = DefaultEnv::GetLog();

    if( sign )
    {
      st = WriteCurrentMessage( sign );
      if( !st.IsOK() || st.code == suRetry )
        return st;
    }

    st = WriteCurrentMessage( toWrite );
    if( !st.IsOK() || st.code == suRetry )
      return st;

    if( pOutHandler->IsRaw() )
    {
      uint32_t bytesWritten = 0;
      st = pOutHandler->WriteMessageBody( pSocket, bytesWritten );
      pOutMsgSize += bytesWritten;
      log->Dump( AsyncSockMsg, "[%s] Wrote %d bytes of message (0x%x) body.",
                 pStreamName.c_str(), bytesWritten, toWrite );
      if( !st.IsOK() || st.code == suRetry )
        return st;
    }

    st = pSocket->Flash();
    if( !st.IsOK() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( AsyncSockMsg, "[%s] Unable to flash the socket: %s",
                  pStreamName.c_str(), XrdSysE2T( st.errNo ) );
    }

    return st;
  }

  //----------------------------------------------------------------------------
  // Got a read readiness event
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnRead()
  {
    //--------------------------------------------------------------------------
    // There is no incoming message currently being processed so we create
    // a new one
    //--------------------------------------------------------------------------
    if( !pIncoming )
    {
      pHeaderDone  = false;
      pIncoming    = new Message();
      pIncHandler  = std::make_pair( (IncomingMsgHandler*)0, false );
      pIncMsgSize  = 0;
    }

    XRootDStatus  st;
    Log    *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We need to read the header first
    //--------------------------------------------------------------------------
    if( !pHeaderDone )
    {
      st = pTransport->GetHeader( pIncoming, pSocket );
      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }

      if( st.code == suRetry )
        return;

      log->Dump( AsyncSockMsg, "[%s] Received message header for 0x%x size: %d",
                pStreamName.c_str(), pIncoming, pIncoming->GetCursor() );
      pIncMsgSize = pIncoming->GetCursor();
      pHeaderDone = true;
      std::pair<IncomingMsgHandler *, bool> raw;
      pIncHandler = pStream->InstallIncHandler( pIncoming, pSubStreamNum );

      if( pIncHandler.first )
      {
        log->Dump( AsyncSockMsg, "[%s] Will use the raw handler to read body "
                   "of message 0x%x", pStreamName.c_str(), pIncoming );
      }
    }

    bool repeat;
    do
    {
      repeat = false;
      //--------------------------------------------------------------------------
      // We need to call a raw message handler to get the data from the socket
      //--------------------------------------------------------------------------
      if( pIncHandler.first )
      {
        uint32_t bytesRead = 0;
        st = pIncHandler.first->ReadMessageBody( pIncoming, pSocket, bytesRead );
        if( !st.IsOK() )
        {
          OnFault( st );
          return;
        }
        pIncMsgSize += bytesRead;

        if( st.code == suRetry )
          return;
      }
      //--------------------------------------------------------------------------
      // No raw handler, so we read the message to the buffer
      //--------------------------------------------------------------------------
      else
      {
        st = pTransport->GetBody( pIncoming, pSocket );
        if( !st.IsOK() )
        {
          OnFault( st );
          return;
        }

        if( st.code == suRetry )
          return;

        pIncMsgSize = pIncoming->GetSize();
      }

      //--------------------------------------------------------------------------
      // Now check if there are some additional raw data to be read
      //--------------------------------------------------------------------------
      if( !pIncHandler.first )
      {
        uint16_t action = pStream->InspectStatusRsp( pIncoming, pSubStreamNum,
                                                     pIncHandler.first );

        if( action & IncomingMsgHandler::Corrupted )
        {
          OnHeaderCorruption();
          return;
        }

        if( action & IncomingMsgHandler::Raw )
        {
          pIncHandler.second = true;
          repeat = true;
        }

        if( action & IncomingMsgHandler::More )
          repeat = true; // for pgwrite we might have additional non-raw data
      }
    }
    while( repeat );

    //--------------------------------------------------------------------------
    // Report the incoming message
    //--------------------------------------------------------------------------
    log->Dump( AsyncSockMsg, "[%s] Received message 0x%x of %d bytes",
               pStreamName.c_str(), pIncoming, pIncMsgSize );

    pStream->OnIncoming( pSubStreamNum, pIncoming, pIncMsgSize );
    pIncoming = 0;
  }

  //----------------------------------------------------------------------------
  // Got a read readiness event while handshaking
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnReadWhileHandshaking()
  {
    //--------------------------------------------------------------------------
    // Read the message and let the transport handler look at it when
    // reading has finished
    //--------------------------------------------------------------------------
    XRootDStatus st = ReadMessage( pHSIncoming );
    if( !st.IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    if( st.code != suDone )
      return;

    HandleHandShake();
  }

  //------------------------------------------------------------------------
  // Handle the handshake message
  //------------------------------------------------------------------------
  void AsyncSocketHandler::HandleHandShake()
  {
    //--------------------------------------------------------------------------
    // OK, we have a new message, let's deal with it;
    //--------------------------------------------------------------------------
    pHandShakeData->in = pHSIncoming;
    pHSIncoming = 0;
    XRootDStatus st = pTransport->HandShake( pHandShakeData, *pChannelData );

    //--------------------------------------------------------------------------
    // Deal with wait responses
    //--------------------------------------------------------------------------
    kXR_int32 waitSeconds = HandleWaitRsp( pHandShakeData->in );

    delete pHandShakeData->in;
    pHandShakeData->in = 0;

    if( !st.IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    //--------------------------------------------------------------------------
    // We are handling a wait response and the transport handler told
    // as to retry the request
    //--------------------------------------------------------------------------
    if( st.code == suRetry )
    {
      if( waitSeconds >=0 )
      {
        time_t resendTime = ::time( 0 ) + waitSeconds;
        if( resendTime > pConnectionStarted + pConnectionTimeout )
        {
          Log *log = DefaultEnv::GetLog();
          log->Error( AsyncSockMsg,
                      "[%s] Won't retry kXR_endsess request because would"
                      "reach connection timeout.",
                      pStreamName.c_str() );

          OnFaultWhileHandshaking( XRootDStatus( stError, errSocketTimeout ) );
        }
        else
        {
          TaskManager *taskMgr = DefaultEnv::GetPostMaster()->GetTaskManager();
          WaitTask *task = new WaitTask( this );
          taskMgr->RegisterTask( task, resendTime );
        }
        return;
      }
      //--------------------------------------------------------------------------
      // We are re-sending a protocol request
      //--------------------------------------------------------------------------
      else if( pHandShakeData->out )
      {
        SendHSMsg();
        return;
      }
    }

    //--------------------------------------------------------------------------
    // If now is the time to enable encryption
    //--------------------------------------------------------------------------
    if( !pSocket->IsEncrypted() &&
         pTransport->NeedEncryption( pHandShakeData, *pChannelData ) )
    {
      XRootDStatus st = DoTlsHandShake();
      if( !st.IsOK() || st.code == suRetry ) return;
    }

    //--------------------------------------------------------------------------
    // Now prepare the next step of the hand-shake procedure
    //--------------------------------------------------------------------------
    HandShakeNextStep( st.IsOK() && st.code == suDone );
  }

  //------------------------------------------------------------------------
  // Prepare the next step of the hand-shake procedure
  //------------------------------------------------------------------------
  void AsyncSocketHandler::HandShakeNextStep( bool done )
  {
    //--------------------------------------------------------------------------
    // We successfully proceeded to the next step
    //--------------------------------------------------------------------------
    ++pHandShakeData->step;

    //--------------------------------------------------------------------------
    // The hand shake process is done
    //--------------------------------------------------------------------------
    if( done )
    {
      delete pHandShakeData;
      pHandShakeData = nullptr;
      hswriter.reset();
      XRootDStatus st;
      if( !(st = EnableUplink()).IsOK() )
      {
        OnFaultWhileHandshaking( st );
        return;
      }
      pHandShakeDone = true;
      pStream->OnConnect( pSubStreamNum );
    }
    //--------------------------------------------------------------------------
    // The transport handler gave us something to write
    //--------------------------------------------------------------------------
    else if( pHandShakeData->out )
    {
      SendHSMsg();
    }
  }

  //----------------------------------------------------------------------------
  // Read a message
  //----------------------------------------------------------------------------
  XRootDStatus AsyncSocketHandler::ReadMessage( Message *&toRead )
  {
    if( !toRead )
    {
      pHeaderDone = false;
      toRead      = new Message();
    }

    XRootDStatus  st;
    Log    *log = DefaultEnv::GetLog();
    if( !pHeaderDone )
    {
      st = pTransport->GetHeader( toRead, pSocket );
      if( st.IsOK() && st.code == suDone )
      {
        log->Dump( AsyncSockMsg,
                  "[%s] Received message header, size: %d",
                  pStreamName.c_str(), toRead->GetCursor() );
        pHeaderDone = true;
      }
      else
        return st;
    }

    st = pTransport->GetBody( toRead, pSocket );
    if( st.IsOK() && st.code == suDone )
    {
      log->Dump( AsyncSockMsg, "[%s] Received a message of %d bytes",
                 pStreamName.c_str(), toRead->GetSize() );
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Handle fault
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnFault( XRootDStatus st )
  {
    Log *log = DefaultEnv::GetLog();
    log->Error( AsyncSockMsg, "[%s] Socket error encountered: %s",
                pStreamName.c_str(), st.ToString().c_str() );

    if( !pIncHandler.second )
      delete pIncoming;

    pIncoming   = 0;
    pOutgoing   = 0;
    pOutHandler = 0;

    pStream->OnError( pSubStreamNum, st );
  }

  //----------------------------------------------------------------------------
  // Handle fault while handshaking
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnFaultWhileHandshaking( XRootDStatus st )
  {
    Log *log = DefaultEnv::GetLog();
    log->Error( AsyncSockMsg, "[%s] Socket error while handshaking: %s",
                pStreamName.c_str(), st.ToString().c_str() );
    delete pHSIncoming;
    pHSIncoming = 0;
    if( hswriter ) hswriter->Reset();

    pStream->OnConnectError( pSubStreamNum, st );
  }

  //----------------------------------------------------------------------------
  // Handle write timeout
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnWriteTimeout()
  {
    pStream->OnWriteTimeout( pSubStreamNum );
  }

  //----------------------------------------------------------------------------
  // Handler read timeout
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnReadTimeout()
  {
    bool isBroken = false;
    pStream->OnReadTimeout( pSubStreamNum, isBroken );

    if( isBroken )
    {
      // if we are here it means Stream::OnError has been
      // called from inside of Stream::OnReadTimeout, this
      // in turn means that the ownership of following
      // pointers, has been transferred to the inQueue
      if( !pIncHandler.second )
        delete pIncoming;

      pIncoming   = 0;
      pOutgoing   = 0;
      pOutHandler = 0;
    }
  }

  //----------------------------------------------------------------------------
  // Handle timeout while handshaking
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnTimeoutWhileHandshaking()
  {
    time_t now = time(0);
    if( now > pConnectionStarted+pConnectionTimeout )
      OnFaultWhileHandshaking( XRootDStatus( stError, errSocketTimeout ) );
  }

  //----------------------------------------------------------------------------
  // Handle header corruption in case of kXR_status response
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnHeaderCorruption()
  {
    //--------------------------------------------------------------------------
    // We need to force a socket error so this is handled in a similar way as
    // a stream t/o and all requests are retried
    //--------------------------------------------------------------------------
    pStream->ForceError( XRootDStatus( stError, errSocketError ) );

    if( !pIncHandler.second )
      delete pIncoming;

    pIncoming   = 0;
    pOutgoing   = 0;
    pOutHandler = 0;
  }

  //------------------------------------------------------------------------
  // Carry out the TLS hand-shake
  //------------------------------------------------------------------------
  XRootDStatus AsyncSocketHandler::DoTlsHandShake()
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( AsyncSockMsg, "[%s] TLS hand-shake exchange.", pStreamName.c_str() );

    XRootDStatus st;
    if( !( st = pSocket->TlsHandShake( this, pUrl.GetHostName() ) ).IsOK() )
    {
      OnFaultWhileHandshaking( st );
      pTlsHandShakeOngoing = false;
      return st;
    }

    if( st.code == suRetry )
    {
      pTlsHandShakeOngoing = true;
      return st;
    }

    pTlsHandShakeOngoing = false;
    log->Info( AsyncSockMsg, "[%s] TLS hand-shake done.", pStreamName.c_str() );

    return st;
  }

  //------------------------------------------------------------------------
  // Handle read/write event if we are in the middle of a TLS hand-shake
  //------------------------------------------------------------------------
  inline void AsyncSocketHandler::OnTLSHandShake()
  {
    XRootDStatus st = DoTlsHandShake();
    if( !st.IsOK() || st.code == suRetry ) return;

    HandShakeNextStep( pTransport->HandShakeDone( pHandShakeData,
                                                  *pChannelData ) );
  }

  void AsyncSocketHandler::SendHSMsg()
  {
    if( !hswriter )
    {
      OnFaultWhileHandshaking( XRootDStatus( stError, errInternal, 0,
                                             "HS writer object missing!" ) );
      return;
    }
    hswriter->Reset( pHandShakeData->out );
    pHandShakeData->out = nullptr;
    XRootDStatus st;
    if( !(st = EnableUplink()).IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }
  }

  kXR_int32 AsyncSocketHandler::HandleWaitRsp( Message *msg )
  {
    // It would be more coherent if this could be done in the
    // transport layer, unfortunately the API does not allow it.
    kXR_int32 waitSeconds = -1;
    ServerResponse *rsp = (ServerResponse*)msg->GetBuffer();
    if( rsp->hdr.status == kXR_wait )
      waitSeconds = rsp->body.wait.seconds;
    return waitSeconds;
  }
}
