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
#include <netinet/tcp.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  AsyncSocketHandler::AsyncSocketHandler( Poller           *poller,
                                          TransportHandler *transport,
                                          AnyObject        *channelData,
                                          uint16_t          subStreamNum ):
    pPoller( poller ),
    pTransport( transport ),
    pChannelData( channelData ),
    pSubStreamNum( subStreamNum ),
    pStream( 0 ),
    pSocket( 0 ),
    pIncoming( 0 ),
    pOutgoing( 0 ),
    pHandShakeData( 0 ),
    pHandShakeDone( false ),
    pConnectionStarted( 0 ),
    pConnectionTimeout( 0 ),
    pHeaderDone( false ),
    pOutMsgDone( false ),
    pOutHandler( 0 ),
    pIncMsgSize( 0 ),
    pOutMsgSize( 0 )
  {
    Env *env = DefaultEnv::GetEnv();

    int timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );
    pTimeoutResolution = timeoutResolution;

    pSocket = new Socket();
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
  }

  //----------------------------------------------------------------------------
  // Connect to given address
  //----------------------------------------------------------------------------
  Status AsyncSocketHandler::Connect( time_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    pLastActivity = pConnectionStarted = ::time(0);
    pConnectionTimeout = timeout;

    //--------------------------------------------------------------------------
    // Initialize the socket
    //--------------------------------------------------------------------------
    Status st = pSocket->Initialize( pSockAddr.Family() );
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
      int    param = 1;
      Status st    = pSocket->SetSockOpt( SOL_SOCKET, SO_KEEPALIVE, &param,
                                          sizeof(param) );
      if( !st.IsOK() )
        log->Error( AsyncSockMsg, "[%s] Unable to turn on keepalive: %s",
                    st.ToString().c_str() );

#if defined(__linux__) && defined( TCP_KEEPIDLE ) && \
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
      Status st( stFatal, errPollerError );
      pSocket->Close();
      return st;
    }

    if( !pPoller->EnableWriteNotification( pSocket, true, pTimeoutResolution ) )
    {
      Status st( stFatal, errPollerError );
      pPoller->RemoveSocket( pSocket );
      pSocket->Close();
      return st;
    }

    return Status();
  }

  //----------------------------------------------------------------------------
  // Close the connection
  //----------------------------------------------------------------------------
  Status AsyncSocketHandler::Close()
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( AsyncSockMsg, "[%s] Closing the socket", pStreamName.c_str() );

    pTransport->Disconnect( *pChannelData, pStream->GetStreamNumber(),
                            pSubStreamNum );

    pPoller->RemoveSocket( pSocket );
    pSocket->Close();

    if( !pIncHandler.second )
      delete pIncoming;

    pIncoming = 0;
    return Status();
  }

  //----------------------------------------------------------------------------
  // Set a stream object to be notified about the status of the operations
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::SetStream( Stream *stream )
  {
    pStream    = stream;
    std::ostringstream o;
    o << pStream->GetURL()->GetHostId();
    o << " #" << pStream->GetStreamNumber();
    o << "." << pSubStreamNum;
    pStreamName = o.str();
  }

  //----------------------------------------------------------------------------
  // Handler a socket event
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::Event( uint8_t type, XrdCl::Socket */*socket*/ )
  {
    //--------------------------------------------------------------------------
    // Read event
    //--------------------------------------------------------------------------
    if( type & ReadyToRead )
    {
      pLastActivity = time(0);
      if( likely( pHandShakeDone ) )
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
    Status st = pSocket->GetSockOpt( SOL_SOCKET, SO_ERROR, &errorCode,
                                     &optSize );

    //--------------------------------------------------------------------------
    // This is an internal error really (either logic or system fault),
    // so we call it a day and don't retry
    //--------------------------------------------------------------------------
    if( !st.IsOK() )
    {
      log->Error( AsyncSockMsg, "[%s] Unable to get the status of the "
                  "connect operation: %s", pStreamName.c_str(),
                  strerror( errno ) );
      pStream->OnConnectError( pSubStreamNum,
                               Status( stFatal, errSocketOptError, errno ) );
      return;
    }

    //--------------------------------------------------------------------------
    // We were unable to connect
    //--------------------------------------------------------------------------
    if( errorCode )
    {
      log->Error( AsyncSockMsg, "[%s] Unable to connect: %s",
                  pStreamName.c_str(), strerror( errorCode ) );
      pStream->OnConnectError( pSubStreamNum,
                               Status( stError, errConnectionError ) );
      return;
    }
    pSocket->SetStatus( Socket::Connected );

    //--------------------------------------------------------------------------
    // Initialize the handshake
    //--------------------------------------------------------------------------
    pHandShakeData = new HandShakeData( pStream->GetURL(),
                                        pStream->GetStreamNumber(),
                                        pSubStreamNum );
    pHandShakeData->serverAddr = &pSocket->GetServerAddress();
    pHandShakeData->clientName = pSocket->GetSockName();
    pHandShakeData->streamName = pStreamName;

    st = pTransport->HandShake( pHandShakeData, *pChannelData );
    ++pHandShakeData->step;

    if( !st.IsOK() )
    {
      log->Error( AsyncSockMsg, "[%s] Connection negotiation failed",
                  pStreamName.c_str() );
      pStream->OnConnectError( pSubStreamNum, st );
      return;
    }

    //--------------------------------------------------------------------------
    // Transport has given us something to send
    //--------------------------------------------------------------------------
    if( pHandShakeData->out )
    {
      pOutgoing = pHandShakeData->out;
      pHandShakeData->out = 0;
    }

    //--------------------------------------------------------------------------
    // Listen to what the server has to say
    //--------------------------------------------------------------------------
    if( !pPoller->EnableReadNotification( pSocket, true, pTimeoutResolution ) )
    {
      pStream->OnConnectError( pSubStreamNum,
                               Status( stFatal, errPollerError ) );
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
    }

    //--------------------------------------------------------------------------
    // Write the message if not already written
    //--------------------------------------------------------------------------
    Status st;
    if( !pOutMsgDone )
    {
      if( !(st = WriteCurrentMessage()).IsOK() )
      {
        OnFault( st );
        return;
      }

      if( st.code == suRetry )
        return;

      Log *log = DefaultEnv::GetLog();

      if( pOutHandler && pOutHandler->IsRaw() )
      {
        log->Dump( AsyncSockMsg, "[%s] Will call raw handler to write payload "
                   "for message: %s (0x%x).", pStreamName.c_str(),
                   pOutgoing->GetDescription().c_str(), pOutgoing );
      }

      pOutMsgDone = true;
    }

    //--------------------------------------------------------------------------
    // Check if the handler needs to be called
    //--------------------------------------------------------------------------
    if( pOutHandler && pOutHandler->IsRaw() )
    {
      uint32_t bytesWritten = 0;
      st = pOutHandler->WriteMessageBody( pSocket->GetFD(), bytesWritten );
      pOutMsgSize += bytesWritten;
      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }

      if( st.code == suRetry )
        return;
    }

    Log *log = DefaultEnv::GetLog();
    log->Dump( AsyncSockMsg, "[%s] Successfully sent message: %s (0x%x).",
               pStreamName.c_str(), pOutgoing->GetDescription().c_str(),
               pOutgoing );

    pStream->OnMessageSent( pSubStreamNum, pOutgoing, pOutMsgSize );
    pOutgoing = 0;
  }

  //----------------------------------------------------------------------------
  // Got a write readiness event while handshaking
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnWriteWhileHandshaking()
  {
    Status st;
    if( !pOutgoing )
    {
      if( !(st = DisableUplink()).IsOK() )
        OnFaultWhileHandshaking( st );
      return;
    }

    if( !(st = WriteCurrentMessage()).IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    if( st.code != suRetry )
    {
      delete pOutgoing;
      pOutgoing = 0;
      if( !(st = DisableUplink()).IsOK() )
        OnFaultWhileHandshaking( st );
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Write the current message
  //----------------------------------------------------------------------------
  Status AsyncSocketHandler::WriteCurrentMessage()
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Try to write down the current message
    //--------------------------------------------------------------------------
    Message  *msg             = pOutgoing;
    uint32_t  leftToBeWritten = msg->GetSize()-msg->GetCursor();

    while( leftToBeWritten )
    {
      int status = pSocket->Send( msg->GetBufferAtCursor(), leftToBeWritten );
      if( status <= 0 )
      {
        //----------------------------------------------------------------------
        // Writing operation would block! So we are done for now, but we will
        // return
        //----------------------------------------------------------------------
        if( errno == EAGAIN || errno == EWOULDBLOCK )
          return Status( stOK, suRetry );

        //----------------------------------------------------------------------
        // Actual socket error error!
        //----------------------------------------------------------------------
        pOutgoing->SetCursor( 0 );
        return Status( stError, errSocketError, errno );
      }
      msg->AdvanceCursor( status );
      leftToBeWritten -= status;
    }

    //--------------------------------------------------------------------------
    // We have written the message successfully
    //--------------------------------------------------------------------------
    log->Dump( AsyncSockMsg, "[%s] Wrote a message: %s (0x%x), %d bytes",
               pStreamName.c_str(), pOutgoing->GetDescription().c_str(),
               pOutgoing, pOutgoing->GetSize() );
    return Status();
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

    Status  st;
    Log    *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We need to read the header first
    //--------------------------------------------------------------------------
    if( !pHeaderDone )
    {
      st = pTransport->GetHeader( pIncoming, pSocket->GetFD() );
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

    //--------------------------------------------------------------------------
    // We need to call a raw message handler to get the data from the socket
    //--------------------------------------------------------------------------
    if( pIncHandler.first )
    {
      uint32_t bytesRead = 0;
      st = pIncHandler.first->ReadMessageBody( pIncoming, pSocket->GetFD(),
                                               bytesRead );
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
      st = pTransport->GetBody( pIncoming, pSocket->GetFD() );
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
    Status st = ReadMessage();
    if( !st.IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    if( st.code != suDone )
      return;

    //--------------------------------------------------------------------------
    // OK, we have a new message, let's deal with it;
    //--------------------------------------------------------------------------
    pHandShakeData->in = pIncoming;
    pIncoming = 0;
    st = pTransport->HandShake( pHandShakeData, *pChannelData );
    ++pHandShakeData->step;
    delete pHandShakeData->in;
    pHandShakeData->in = 0;

    if( !st.IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    //--------------------------------------------------------------------------
    // The transport handler gave us something to write
    //--------------------------------------------------------------------------
    if( pHandShakeData->out )
    {
      pOutgoing = pHandShakeData->out;
      pHandShakeData->out = 0;
      Status st;
      if( !(st = EnableUplink()).IsOK() )
      {
        OnFaultWhileHandshaking( st );
        return;
      }
    }

    //--------------------------------------------------------------------------
    // The hand shake process is done
    //--------------------------------------------------------------------------
    if( st.IsOK() && st.code == suDone )
    {
      delete pHandShakeData;
      if( !(st = EnableUplink()).IsOK() )
      {
        OnFaultWhileHandshaking( Status( stFatal, errPollerError ) );
        return;
      }
      pHandShakeDone = true;
      pStream->OnConnect( pSubStreamNum );
    }

    if( !st.IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Read a message
  //----------------------------------------------------------------------------
  Status AsyncSocketHandler::ReadMessage()
  {
    if( !pIncoming )
    {
      pHeaderDone = false;
      pIncoming   = new Message();
    }

    Status  st;
    Log    *log = DefaultEnv::GetLog();
    if( !pHeaderDone )
    {
      st = pTransport->GetHeader( pIncoming, pSocket->GetFD() );
      if( st.IsOK() && st.code == suDone )
      {
        log->Dump( AsyncSockMsg,
                  "[%s] Received message header, size: %d",
                  pStreamName.c_str(), pIncoming->GetCursor() );
        pHeaderDone = true;
      }
      else
        return st;
    }

    st = pTransport->GetBody( pIncoming, pSocket->GetFD() );
    if( st.IsOK() && st.code == suDone )
    {
      log->Dump( AsyncSockMsg, "[%s] Received a message of %d bytes",
                 pStreamName.c_str(), pIncoming->GetSize() );
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Handle fault
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnFault( Status st )
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
  void AsyncSocketHandler::OnFaultWhileHandshaking( Status st )
  {
    Log *log = DefaultEnv::GetLog();
    log->Error( AsyncSockMsg, "[%s] Socket error while handshaking: %s",
                pStreamName.c_str(), st.ToString().c_str() );
    delete pIncoming;
    delete pOutgoing;
    pIncoming = 0;
    pOutgoing = 0;

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
    pStream->OnReadTimeout( pSubStreamNum );
  }

  //----------------------------------------------------------------------------
  // Handle timeout while handshaking
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnTimeoutWhileHandshaking()
  {
    time_t now = time(0);
    if( now > pConnectionStarted+pConnectionTimeout )
      OnFaultWhileHandshaking( Status( stError, errSocketTimeout ) );
  }
}
