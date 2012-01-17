//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClChannel.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include <sys/types.h>
#include <sys/socket.h>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Message helper
  //----------------------------------------------------------------------------
  struct OutMessageHelper
  {
    OutMessageHelper( Message *message, MessageStatusHandler *hndlr ):
      msg( message ), handler( hndlr )  {}
    Message              *msg;
    MessageStatusHandler *handler;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  Stream::Stream( const URL *url, uint16_t streamNum ):
    pUrl( url ),
    pStreamNum( streamNum ),
    pTransport( 0 ),
    pSocket( 0 ),
    pPoller( 0 ),
    pTaskManager( 0 ),
    pCurrentOut( 0 ),
    pIncomingQueue( 0 ),
    pIncoming( 0 ),
    pStreamStatus( Disconnected ),
    pChannelData( 0 ),
    pLastStreamError( 0 ),
    pLastActivity( 0 ),
    pErrorTime( 0 ),
    pHandShakeData( 0 ),
    pConnectionCount( 0 ),
    pConnectionInitTime( 0 )
  {
    pSocket = new Socket();

    Env *env = DefaultEnv::GetEnv();

    int timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );
    pTimeoutResolution = timeoutResolution;

    int connectionWindow = DefaultConnectionWindow;
    env->GetInt( "ConnectionWindow", connectionWindow );
    pConnectionWindow = connectionWindow;

    int connectionRetry = DefaultConnectionRetry;
    env->GetInt( "ConnectionRetry", connectionRetry );
    pConnectionRetry = connectionRetry;

    int streamErrorWindow = DefaultStreamErrorWindow;
    env->GetInt( "StreamErrorWindow", streamErrorWindow );
    pStreamErrorWindow = streamErrorWindow;
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Stream::~Stream()
  {
    delete pSocket;
  }

  //----------------------------------------------------------------------------
  // Handle a socket event
  //----------------------------------------------------------------------------
  void Stream::Event( uint8_t type, Socket *socket )
  {
    switch( type )
    {
      case ReadyToRead:
        pLastActivity = ::time(0);
        if( pStreamStatus == Connected )
          ConnectedReadyToRead();
        else
          ConnectingReadyToRead();
        break;
      case ReadTimeOut:
        if( pStreamStatus == Connected )
          HandleReadTimeout();
        else
          HandleConnectingTimeout();
        break;
      case ReadyToWrite:
        pLastActivity = ::time(0);
        if( pStreamStatus == Connected )
          ConnectedReadyToWrite();
        else
          ConnectingReadyToWrite();
        break;
      case WriteTimeOut:
        if( pStreamStatus == Connected )
          HandleWriteTimeout();
        else
          HandleConnectingTimeout();
        break;
    }
  }

  //----------------------------------------------------------------------------
  // Queue the message for sending
  //----------------------------------------------------------------------------
  Status Stream::QueueOut( Message              *msg,
                           MessageStatusHandler *handler,
                           uint32_t              timeout )
  {
    //--------------------------------------------------------------------------
    // Check if the stream is connected and if it may be reconnected
    //--------------------------------------------------------------------------
    Status sc;
    if( !(sc = CheckConnection()).IsOK() )
    {
      handler->HandleStatus( msg, sc );
      return sc;
    }

    //--------------------------------------------------------------------------
    // The stream seems to be OK
    //--------------------------------------------------------------------------
    pMutex.Lock();
    if( pOutQueue.empty() )
    {
      if(!pPoller->EnableWriteNotification( pSocket, true, pTimeoutResolution ))
      {
        HandleStreamFault( errPollerError );
        return Status( stError, errPollerError );
      }
    }

    pOutQueue.push_back( new OutMessageHelper( msg, handler )  );
    pMutex.UnLock();
    return Status();
  }

  //----------------------------------------------------------------------------
  // Establish the connection if needed
  //----------------------------------------------------------------------------
  Status Stream::CheckConnection()
  {
    Log *log = Utils::GetDefaultLog();
    time_t now = ::time(0);

    XrdSysMutexHelper scopedLock( pMutex );

    if( pStreamStatus == Connected || pStreamStatus == Connecting )
      return Status();

    if( pStreamStatus == Error && (now-pErrorTime > pStreamErrorWindow) )
      return Status( stError );

    return Connect();
  }

  //----------------------------------------------------------------------------
  // Start the async connection process
  //----------------------------------------------------------------------------
  Status Stream::Connect()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = Utils::GetDefaultLog();

    pConnectionInitTime = ::time(0);
    ++pConnectionCount;

    //--------------------------------------------------------------------------
    // We're disconnected so we need to connect
    //--------------------------------------------------------------------------
    Status st = pSocket->Initialize();
    if( !st.IsOK() )
    {
      log->Error( PostMasterMsg, "[%s #%d] Unable to initialize socket: %s",
                                 pUrl->GetHostId().c_str(), pStreamNum,
                                 strerror( st.errNo ) );
      pStreamStatus = Error;
      return st;
    }

    st = pSocket->Connect( pUrl->GetHostName(), pUrl->GetPort(), 0 );
    if( !st.IsOK() )
    {
      log->Error( PostMasterMsg, "[%s #%d] Unable to initiate the connection: %s",
                                 pUrl->GetHostId().c_str(), pStreamNum,
                                 strerror( st.errNo ) );
      pStreamStatus = Error;
      return st;
    }
    pStreamStatus = Connecting;

    //--------------------------------------------------------------------------
    // We should get the ready to write event once we're really connected
    // so we need to listen to it
    //--------------------------------------------------------------------------
    if( !pPoller->AddSocket( pSocket, this ) )
    {
      HandleStreamFault( errPollerError );
      return Status( stError, errPollerError );
    }

    if( !pPoller->EnableWriteNotification( pSocket, pTimeoutResolution ) )
    {
      HandleStreamFault( errPollerError );
      return Status( stError, errPollerError );
    }

    return Status();
  }

  //----------------------------------------------------------------------------
  // Disconnect the stream
  //----------------------------------------------------------------------------
  void Stream::Disconnect()
  {
    Log *log = Utils::GetDefaultLog();
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // We need to check here (in a locked section) if the queue is empty,
    // if it's not, then somebody has requested message sending, so we cancel
    // the disconnection
    //--------------------------------------------------------------------------
    if( !pOutQueue.empty() )
      return;

    //--------------------------------------------------------------------------
    // We don't seem to have anything to send so we disconnect
    //--------------------------------------------------------------------------
    log->Debug( PostMasterMsg, "[%s #%d] Connection TTL elapsed, disconnecting.",
                               pUrl->GetHostId().c_str(), pStreamNum );
    pPoller->RemoveSocket( pSocket );
    pSocket->Close();

    //!!! Fail all the receivers
    pStreamStatus = Disconnected;
  }

  //----------------------------------------------------------------------------
  // Handle a clock event
  //----------------------------------------------------------------------------
  void Stream::Tick( time_t now )
  {
  }

  //----------------------------------------------------------------------------
  // Handle the socket readiness to write in the connection stage
  //----------------------------------------------------------------------------
  void Stream::ConnectingReadyToWrite()
  {
    Log *log = Utils::GetDefaultLog();

    //--------------------------------------------------------------------------
    // We got a write event while being in the 'Connecting' state, if the
    // socket's state is also 'Connecting' it means that we just got the
    // async connect return so we need to verify whether the connection
    // was successful or not
    //--------------------------------------------------------------------------
    if( pSocket->GetStatus() == Socket::Connecting )
    {
      int errorCode = 0;
      socklen_t optSize = sizeof( errorCode );
      Status st = pSocket->GetSockOpt( SOL_SOCKET, SO_ERROR, &errorCode,
                                       &optSize );

      //------------------------------------------------------------------------
      // This is an internal error really (either logic or system fault), 
      // so we call it a day and don't retry
      //------------------------------------------------------------------------
      if( !st.IsOK() )
      {
        log->Error( PostMasterMsg, "[%s #%d] Unable to get the status of the "
                                   "connect operation: %s",
                                   pUrl->GetHostId().c_str(), pStreamNum,
                                   strerror( errno ) );
        HandleStreamFault( errSocketOptError );
        return;
      }

      //------------------------------------------------------------------------
      // Check the error code from the socket
      //------------------------------------------------------------------------
      if( errorCode )
      {
        log->Error( PostMasterMsg, "[%s #%d] Unable to connect: %s",
                                   pUrl->GetHostId().c_str(), pStreamNum,
                                   strerror( errorCode ) );
        HandleStreamFault( errSocketError );
        return;
      }

      pSocket->SetStatus( Socket::Connected );
      pHandShakeData = new HandShakeData( pUrl, pStreamNum );

      //------------------------------------------------------------------------
      // Call the protocol handshake method for the first time
      // to see whether we have something to send out
      //------------------------------------------------------------------------
      while(1)
      {
        st = pTransport->HandShake( pHandShakeData, *pChannelData );
        ++pHandShakeData->step;

        if( !st.IsOK() )
        {
          log->Error( PostMasterMsg, "[%s #%d] Connection negotiation failed",
                                      pUrl->GetHostId().c_str(), pStreamNum );
          HandleStreamFault( st.code );
          return;
        }

        //----------------------------------------------------------------------
        // We do have something, so we queue it for sending
        //----------------------------------------------------------------------
        if( pHandShakeData->out )
        {
          pOutQueueConnect.push_back( new OutMessageHelper( pHandShakeData->out, 0 ) );
          pHandShakeData->out = 0;
        }

        if( st.code != suRetry )
          break;
      }

      if( !pPoller->EnableReadNotification( pSocket, true, pTimeoutResolution ) )
      {
        HandleStreamFault( errPollerError );
        return;
      }

      //------------------------------------------------------------------------
      // We're done handshaking - we're setting the stream status to connected
      // and reseting the connection counter to zero so that we could again
      // reconnect as many times as needed on the next timeout
      //------------------------------------------------------------------------
      if( st.IsOK() && st.code == suDone )
      {
        pConnectionCount = 0;
        pStreamStatus    = Connected;
      }
    }

    //--------------------------------------------------------------------------
    // If we're here it means that we should have a message in the outgoing
    // buffer, we don't then we disable the write notifications
    //--------------------------------------------------------------------------
    Status st = WriteMessage( pOutQueueConnect );

    if( !st.IsOK() )
    {
      HandleStreamFault( st.code );
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Handle the ReadyToWrite message
  //----------------------------------------------------------------------------
  void Stream::ConnectedReadyToWrite()
  {
    Log *log = Utils::GetDefaultLog();
    Status st = WriteMessage( pOutQueue );
    if( !st.IsOK() )
    {
      HandleStreamFault( st.code );
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Write a message from an outgoing queue
  //----------------------------------------------------------------------------
  Status Stream::WriteMessage( std::list<OutMessageHelper *> &queue )
  {
    Log *log = Utils::GetDefaultLog();

    //--------------------------------------------------------------------------
    // Pick up a message if we're not in process of writing something
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );

    if( !pCurrentOut )
    {
      if( !queue.empty() )
      {
        pCurrentOut = queue.front();
        pCurrentOut->msg->SetCursor(0);
      }
      else
      {
        if( !pPoller->EnableWriteNotification( pSocket, false ) )
          return Status( stError, errPollerError );
        return Status();
      }
    }
    scopedLock.UnLock();

    //--------------------------------------------------------------------------
    // Try to write down the current message
    //--------------------------------------------------------------------------
    int       sock            = pSocket->GetFD();
    Message  *msg             = pCurrentOut->msg;
    uint32_t  leftToBeWritten = msg->GetSize()-msg->GetCursor();

    while( leftToBeWritten )
    {
      int status = ::write( sock, msg->GetBufferAtCursor(), leftToBeWritten );
      if( status <= 0 )
      {
        //----------------------------------------------------------------------
        // Writing operation would block!
        //----------------------------------------------------------------------
        if( errno == EAGAIN || errno == EWOULDBLOCK )
          return Status( stOK, suContinue );

        //----------------------------------------------------------------------
        // Actual socket error error!
        //----------------------------------------------------------------------
        pCurrentOut->msg->SetCursor( 0 );
        return Status( stError, errSocketError, errno );
      }
      msg->AdvanceCursor( status );
      leftToBeWritten -= status;
    }

    //--------------------------------------------------------------------------
    // We have written the message successfully
    //--------------------------------------------------------------------------
    log->Dump( PostMasterMsg, "[%s #%d] Wrote a message of %d bytes",
                              pUrl->GetHostId().c_str(), pStreamNum,
                              pCurrentOut->msg->GetSize() );

    if( pCurrentOut->handler )
      pCurrentOut->handler->HandleStatus( pCurrentOut->msg, Status() );

    delete pCurrentOut;
    pCurrentOut = 0;

    scopedLock.Lock( &pMutex );
    queue.pop_front();
    if( queue.empty() )
    {
      log->Dump( PostMasterMsg, "[%s #%d] Nothing to write, disable write "
                                "notifications",
                                 pUrl->GetHostId().c_str(), pStreamNum );

      if( !pPoller->EnableWriteNotification( pSocket, false ) )
        return Status( stError, errPollerError );
    }
    return Status();
  }

  //----------------------------------------------------------------------------
  // Handle the socket readiness to read in the connection stage
  //----------------------------------------------------------------------------
  void Stream::ConnectingReadyToRead()
  {
    //--------------------------------------------------------------------------
    // Read the message and let the transport handler look at it
    //--------------------------------------------------------------------------
    Status st = ReadMessage();
    if( st.IsOK() && st.code == suDone )
    {
      pHandShakeData->in = pIncoming;
      pIncoming = 0;
      Status st = pTransport->HandShake( pHandShakeData, *pChannelData );
      ++pHandShakeData->step;

      if( !st.IsOK() )
      {
        HandleStreamFault( st.code );
        return;
      }

      //------------------------------------------------------------------------
      // The transport handler gave us something to write
      //------------------------------------------------------------------------
      if( pHandShakeData->out )
      {
          pOutQueueConnect.push_back( new OutMessageHelper( pHandShakeData->out, 0 ) );
          pHandShakeData->out = 0;
          if( !pPoller->EnableWriteNotification( pSocket, true, pTimeoutResolution ) )
          {
            HandleStreamFault( errPollerError );
            return;
          }
      }

      //------------------------------------------------------------------------
      // The hand shake process is done
      //------------------------------------------------------------------------
      if( st.IsOK() && st.code == suDone )
      {
        pStreamStatus = Connected;
        pConnectionCount = 0;
        if( !pPoller->EnableWriteNotification( pSocket, true, pTimeoutResolution ) )
        {
          HandleStreamFault( errPollerError );
          return;
        }
      }
    }

    if( !st.IsOK() )
    {
      HandleStreamFault( st.code );
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Handle the socket readiness to read in the connection stage
  //----------------------------------------------------------------------------
  void Stream::ConnectedReadyToRead()
  {
    Status st = ReadMessage();
    if( st.IsOK() && st.code == suDone )
    {
      pIncomingQueue->AddMessage( pIncoming );
      pIncoming = 0;
    }

    if( !st.IsOK() )
    {
      HandleStreamFault( st.code );
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Read message
  //----------------------------------------------------------------------------
  Status Stream::ReadMessage()
  {
    Log *log = Utils::GetDefaultLog();

    if( !pIncoming )
      pIncoming = new Message();

    Status sc = pTransport->GetMessage( pIncoming, pSocket );

    //--------------------------------------------------------------------------
    // The entire message has been read fine
    //--------------------------------------------------------------------------
    if( sc.IsOK() && sc.code == suDone )
    {
      log->Dump( PostMasterMsg, "[%s #%d] Got a message of %d bytes",
                                pUrl->GetHostId().c_str(), pStreamNum,
                                pIncoming->GetSize() );
    }
    return sc;
  }

  //----------------------------------------------------------------------------
  // Handle a timeout during the link negotiation process
  //----------------------------------------------------------------------------
  void Stream::HandleConnectingTimeout()
  {
    time_t now = ::time(0);
    Tick( now );
    if( now >= pConnectionInitTime+pConnectionWindow )
      HandleStreamFault( errSocketTimeout );
  }

  //----------------------------------------------------------------------------
  // Handle read timeout
  //----------------------------------------------------------------------------
  void Stream::HandleReadTimeout()
  {
    time_t now = ::time(0);
    Tick( now );
    if( pTransport->IsStreamTTLElapsed( now-pLastActivity, *pChannelData ) )
      Disconnect();
  }

  //----------------------------------------------------------------------------
  // Handle write timeout
  //----------------------------------------------------------------------------
  void Stream::HandleWriteTimeout()
  {
    time_t now = ::time(0);
    Tick( now );
    if( pTransport->IsStreamTTLElapsed( now-pLastActivity, *pChannelData ) )
      Disconnect();
  }
}

//------------------------------------------------------------------------------
// Handle message timeouts and reconnection in the future
//------------------------------------------------------------------------------
namespace
{
  class StreamConnectorTask: public XrdClient::Task
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StreamConnectorTask( XrdClient::Stream *stream,
                           time_t connectTime,
                           uint16_t timeoutResolution ):
        pStream( stream ), pConnectTime( connectTime ),
        pTimeoutResolution( timeoutResolution ) {}

      //------------------------------------------------------------------------
      // Run the task
      //------------------------------------------------------------------------
      time_t Run( time_t now )
      {
        pStream->Tick( now );
        if( now >= pConnectTime )
        {
          pStream->Connect();
          return 0;
        }

        //----------------------------------------------------------------------
        // Calculate when to run next
        //----------------------------------------------------------------------
        if( pConnectTime < now+pTimeoutResolution )
          return pConnectTime;
        else
          return now+pTimeoutResolution;
      }

    private:
      XrdClient::Stream *pStream;
      time_t             pConnectTime;
      uint16_t           pTimeoutResolution;
  };
}

namespace XrdClient
{
  //------------------------------------------------------------------------
  // Handle stream fault
  //------------------------------------------------------------------------
  void Stream::HandleStreamFault( uint16_t error )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log    *log = Utils::GetDefaultLog();
    time_t  now = ::time(0);

    log->Error( PostMasterMsg, "[%s #%d] Stream fault. Cleaning up.",
                               pUrl->GetHostId().c_str(), pStreamNum );

    pPoller->RemoveSocket( pSocket );
    pSocket->Close();

    //--------------------------------------------------------------------------
    // Check if we are in the connection stage and should retry establishing
    // the connection
    //--------------------------------------------------------------------------
    if( pConnectionCount < pConnectionRetry )
    {
      pStreamStatus = Connecting;
      time_t  newConnectTime = pConnectionInitTime + pConnectionWindow;
      int64_t timeToConnect = newConnectTime - now;
      time_t  scheduleTime   = newConnectTime;
      if( newConnectTime > now+pTimeoutResolution )
        scheduleTime = now+pTimeoutResolution;

      //------------------------------------------------------------------------
      // We may attempt the reconnection process immediately
      //------------------------------------------------------------------------
      if( timeToConnect <= 0 )
      {
        log->Info( PostMasterMsg, "[%s #%d] Attempting reconnection now.",
                                  pUrl->GetHostId().c_str(), pStreamNum );

        Connect();
      }
      //------------------------------------------------------------------------
      // We need to schedule a task to reconnect in the future
      //------------------------------------------------------------------------
      else
      {
        log->Info( PostMasterMsg, "[%s #%d] Attempting reconnection in %d "
                                  "seconds.",
                                  pUrl->GetHostId().c_str(), pStreamNum,
                                  timeToConnect );

        Task *task = new ::StreamConnectorTask( this,
                                                newConnectTime,
                                                pTimeoutResolution );
        pTaskManager->RegisterTask( task, scheduleTime );
      }
      return;
    }

    //--------------------------------------------------------------------------
    // We cannot really do anything - declare and error and fail all the
    // requests
    //--------------------------------------------------------------------------
    pStreamStatus = Error;
    pLastStreamError = error;
    pErrorTime = now;
    //!!! fail all the requests
  }
}
