//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClAsyncSocketHandler.hh"
#include "XrdSys/XrdSysDNS.hh"

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
    pStatus( Socket::Disconnected ),
    pHandShakeData( 0 ),
    pConnectionStarted( 0 ),
    pConnectionTimeout( 0 )
  {
    Env *env = DefaultEnv::GetEnv();

    int timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );
    pTimeoutResolution = timeoutResolution;

    memset( &pSockAddr, 0, sizeof( pSockAddr ) );
    pSocket = new Socket();
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  AsyncSocketHandler::~AsyncSocketHandler()
  {
    delete pSocket;
  }

  //----------------------------------------------------------------------------
  // Connect to gien address
  //----------------------------------------------------------------------------
  Status AsyncSocketHandler::Connect( time_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    pConnectionStarted = ::time(0);
    pConnectionTimeout = timeout;

    if( pStatus == Socket::Connected || pStatus == Socket::Connecting )
      return Status( stOK, suAlreadyDone );

    //--------------------------------------------------------------------------
    // Initialize the socket
    //--------------------------------------------------------------------------
    Status st = pSocket->Initialize();
    if( !st.IsOK() )
    {
      log->Error( PostMasterMsg, "[%s] Unable to initialize socket: %s",
                                 pStreamName.c_str(),
                                 st.ToString().c_str() );
      return st;
    }

    //--------------------------------------------------------------------------
    // Initiate async connection to the address
    //--------------------------------------------------------------------------
    char nameBuff[256];
    XrdSysDNS::IPFormat( (sockaddr*)&pSockAddr, nameBuff, sizeof(nameBuff) );
    log->Debug( PostMasterMsg, "[%s] Attempting connection to %s",
                               pStreamName.c_str(), nameBuff );

    st = pSocket->ConnectToAddress( pSockAddr, 0 );
    if( !st.IsOK() )
    {
      log->Error( PostMasterMsg, "[%s] Unable to initiate the connection: %s",
                                 pStreamName.c_str(), st.ToString().c_str() );
      pStatus = Socket::Disconnected;
      return st;
    }

    //--------------------------------------------------------------------------
    // We should get the ready to write event once we're really connected
    // so we need to listen to it
    //--------------------------------------------------------------------------
    if( !pPoller->AddSocket( pSocket, this ) )
    {
      Status st( stFatal, errPollerError );
      pSocket->Close();
      pStatus = Socket::Disconnected;
      return st;
    }

    if( !pPoller->EnableWriteNotification( pSocket, pTimeoutResolution ) )
    {
      Status st( stFatal, errPollerError );
      pPoller->RemoveSocket( pSocket );
      pSocket->Close();
      pStatus = Socket::Disconnected;
      return st;
    }

    pStatus = Socket::Connecting;
    return Status();
  }

  //----------------------------------------------------------------------------
  // Close the connection
  //----------------------------------------------------------------------------
  Status AsyncSocketHandler::Close()
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( PostMasterMsg, "[%s] Closing the socket", pStreamName.c_str() );

    pTransport->Disconnect( *pChannelData, pStream->GetStreamNumber(),
                            pSubStreamNum );

    pPoller->RemoveSocket( pSocket );
    pSocket->Close();
    pStatus = Socket::Disconnected;
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
    if( pSubStreamNum )
      o << "." << pSubStreamNum;
    pStreamName = o.str();
  }

  //----------------------------------------------------------------------------
  // Handler a socket event
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::Event( uint8_t type, XrdCl::Socket */*socket*/ )
  {
    switch( type )
    {
      //------------------------------------------------------------------------
      // Read event
      //------------------------------------------------------------------------
      case ReadyToRead:
        if( pStatus == Socket::Connecting )
          OnReadWhileHandshaking();
        else
          OnRead();
        break;

      //------------------------------------------------------------------------
      // Read timeout
      //------------------------------------------------------------------------
      case ReadTimeOut:
        if( pStatus == Socket::Connecting )
          OnTimeoutWhileHandshaking();
        else
          OnReadTimeout();
        break;

      //------------------------------------------------------------------------
      // Write event
      //------------------------------------------------------------------------
      case ReadyToWrite:
        if( pSocket->GetStatus() == Socket::Connecting )
          OnConnectionReturn();
        else if( pStatus == Socket::Connecting )
          OnWriteWhileHandshaking();
        else
          OnWrite();
        break;

      //------------------------------------------------------------------------
      // Write timeout
      //------------------------------------------------------------------------
      case WriteTimeOut:
        if( pStatus == Socket::Connecting )
          OnTimeoutWhileHandshaking();
        else
          OnWriteTimeout();
        break;
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
    log->Debug( PostMasterMsg, "[%s] Async connection call returned",
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
      log->Error( PostMasterMsg, "[%s] Unable to get the status of the "
                                 "connect operation: %s",
                                 pStreamName.c_str(), strerror( errno ) );
      pStream->OnConnectError( pSubStreamNum,
                               Status( stFatal, errSocketOptError, errno ) );
      return;
    }

    //--------------------------------------------------------------------------
    // We were unable to connect
    //--------------------------------------------------------------------------
    if( errorCode )
    {
      log->Error( PostMasterMsg, "[%s] Unable to connect: %s",
                                 pStreamName.c_str(),
                                 strerror( errorCode ) );
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
    pHandShakeData->serverAddr = pSocket->GetServerAddress();
    pHandShakeData->clientName = pSocket->GetSockName();
    pHandShakeData->streamName = pStreamName;

    st = pTransport->HandShake( pHandShakeData, *pChannelData );
    ++pHandShakeData->step;

    if( !st.IsOK() )
    {
      log->Error( PostMasterMsg, "[%s] Connection negotiation failed",
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
                               Status( stError, errPollerError ) );
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
      pOutgoing = pStream->OnReadyToWrite( pSubStreamNum );
      if( !pOutgoing )
      {

        Status st = DisableUplink();
        if( !st.IsOK() )
          OnFault( st );
        return;
      }
      pOutgoing->SetCursor( 0 );
    }

    //--------------------------------------------------------------------------
    // Write the message and notify the handler if done
    //--------------------------------------------------------------------------
    Status st;
    if( !(st = WriteCurrentMessage()).IsOK() )
    {
      OnFault( st );
      return;
    }

    // if we're not done we need to get back here
    if( st.code == suContinue )
      return;

    pStream->OnMessageSent( pSubStreamNum, pOutgoing );
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

    if( st.code != suContinue )
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
    int       sock            = pSocket->GetFD();
    Message  *msg             = pOutgoing;
    uint32_t  leftToBeWritten = msg->GetSize()-msg->GetCursor();

    while( leftToBeWritten )
    {
      int status = ::write( sock, msg->GetBufferAtCursor(), leftToBeWritten );
      if( status <= 0 )
      {
        //----------------------------------------------------------------------
        // Writing operation would block! So we are done for now, but we will
        // return
        //----------------------------------------------------------------------
        if( errno == EAGAIN || errno == EWOULDBLOCK )
          return Status( stOK, suContinue );

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
    log->Dump( PostMasterMsg, "[%s] Wrote a message of %d bytes",
                              pStreamName.c_str(),
                              pOutgoing->GetSize() );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Got a read readiness event
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnRead()
  {
    Status st = ReadMessage();
    if( !st.IsOK() )
    {
      OnFault( st );
      return;
    }

    if( st.code != suDone )
      return;

    pStream->OnIncoming( pSubStreamNum, pIncoming );
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
      pIncoming = new Message();

    Status st = pTransport->GetMessage( pIncoming, pSocket );
    if( st.IsOK() && st.code == suDone )
    {
      Log *log = DefaultEnv::GetLog();
      log->Dump( PostMasterMsg, "[%s] Received a message of %d bytes",
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
    log->Error( PostMasterMsg, "[%s] Socket error encountered: %s",
                               pStreamName.c_str(),
                               st.ToString().c_str() );
    delete pIncoming;
    pIncoming = 0;
    pOutgoing = 0;

    pStream->OnError( pSubStreamNum, st );
  }

  //----------------------------------------------------------------------------
  // Handle fault while handshaking
  //----------------------------------------------------------------------------
  void AsyncSocketHandler::OnFaultWhileHandshaking( Status st )
  {
    Log *log = DefaultEnv::GetLog();
    log->Error( PostMasterMsg, "[%s] Socket error while handshaking: %s",
                               pStreamName.c_str(),
                               st.ToString().c_str() );
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
