//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClChannel.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"

#include <ctime>

namespace
{
  //----------------------------------------------------------------------------
  // Filter handler
  //----------------------------------------------------------------------------
  class FilterHandler: public XrdClient::MessageHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      FilterHandler( XrdClient::MessageFilter *filter ):
        pFilter( filter ), pMsg( 0 ), pSem( 0 )
      {
      }

      //------------------------------------------------------------------------
      // Message handler
      //------------------------------------------------------------------------
      virtual uint8_t HandleMessage( XrdClient::Message *msg )
      {
        if( pFilter->Filter( msg ) )
        {
          pMsg = msg;
          pSem.Post();
          return Take | RemoveHandler;
        }
        return Ignore;
      }

      //------------------------------------------------------------------------
      // Wait for the arraival of the message
      //------------------------------------------------------------------------
      XrdClient::Message *WaitForMessage()
      {
        pSem.Wait();
        return pMsg;
      }

    private:
      XrdSysSemaphore           pSem;
      XrdClient::MessageFilter *pFilter;
      XrdClient::Message       *pMsg;
  };

  //----------------------------------------------------------------------------
  // Status handler
  //----------------------------------------------------------------------------
  class StatusHandler: public XrdClient::MessageStatusHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StatusHandler( XrdClient::Message *msg ): pMsg( msg ), pSem( 0 ) {}

      //------------------------------------------------------------------------
      // Handle the status information
      //------------------------------------------------------------------------
      void HandleStatus( const XrdClient::Message *message,
                         XrdClient::Status         status )
      {
        if( pMsg == message )
          pStatus = status;
        pSem.Post();
      }

      //------------------------------------------------------------------------
      // Wait for the status to be ready
      //------------------------------------------------------------------------
      XrdClient:: Status WaitForStatus()
      {
        pSem.Wait();
        return pStatus;
      }
      
    private:
      XrdSysSemaphore     pSem;
      XrdClient::Status   pStatus;
      XrdClient::Message *pMsg;
  };
}

namespace XrdClient
{

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  Channel::Channel( const URL        &url,
                    Poller           *poller,
                    TransportHandler *transport ):
    pUrl( url ),
    pPoller( poller ),
    pTransport( transport ),
    pData( 0 )
  {
    Env *env = DefaultEnv::GetEnv();
    Log *log = Utils::GetDefaultLog();

    int  numStreams = DefaultStreamsPerChannel;
    env->GetInt( "StreamsPerChannel", numStreams );

    log->Debug( PostMasterMsg, "Creating new channel to: %s %d stream(s)",
                                url.GetHostId().c_str(), numStreams );

    pStreams.resize( numStreams );
    for( int i = 0; i < numStreams; ++i )
      pStreams[i] = new Stream( this, i, transport, new Socket(),
                                poller, &pIncoming );

    pTransport->InitializeChannel( pData );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Channel::~Channel()
  {
    pTransport->FinalizeChannel( pData );
    for( int i = 0; i < pStreams.size(); ++i )
      delete pStreams[i];
  }

  //----------------------------------------------------------------------------
  // Send a message synchronously
  //----------------------------------------------------------------------------
  Status Channel::Send( Message *msg, int32_t timeout )
  {
    StatusHandler sh( msg );
    Status sc = Send( msg, &sh, timeout );
    if( !sc.IsOK() )
      return sc;
    sc = sh.WaitForStatus();
    return sc;
  }

  //----------------------------------------------------------------------------
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  Status Channel::Send( Message              *msg,
                        MessageStatusHandler *statusHandler,
                        int32_t               timeout )

  {
    Log *log = Utils::GetDefaultLog();
    uint16_t stream = pTransport->Multiplex( msg, pData );
    log->Dump( PostMasterMsg, "[%s #%d] Sending message %x",
                              pUrl.GetHostId().c_str(), stream, msg );
    return pStreams[stream]->QueueOut( msg, statusHandler, timeout );
  }

  //----------------------------------------------------------------------------
  // Synchronously receive a message - blocks until a message maching
  //----------------------------------------------------------------------------
  Status Channel::Receive( Message       *&msg,
                           MessageFilter  *filter,
                           uint16_t        timeout )
  {
    FilterHandler fh( filter );
    Status sc = Receive( &fh, timeout );
    if( !sc.IsOK() )
      return sc;
    msg = fh.WaitForMessage();
    return Status();
  }

  //----------------------------------------------------------------------------
  // Listen to incomming messages
  //----------------------------------------------------------------------------
  Status Channel::Receive( MessageHandler *handler, uint16_t timeout )
  {
    pIncoming.AddMessageHandler( handler );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Handle connection issue
  //----------------------------------------------------------------------------
  Status Channel::HandleStreamFault( uint16_t sNum )
  {
    //--------------------------------------------------------------------------
    // Check if we need to reestablish the control connection
    //--------------------------------------------------------------------------
    if( sNum != 0 && pTransport->NeedControlConnection() )
      HandleStreamFault( 0 );

    //--------------------------------------------------------------------------
    // Lock the channel and the stream so that nobody else writes to our socket
    //--------------------------------------------------------------------------
    pMutex.Lock();
    pStreams[sNum]->Lock();

    //--------------------------------------------------------------------------
    // The connection might have been reestablished by another thread, in
    // which case we do nothing
    //--------------------------------------------------------------------------
    if( pStreams[sNum]->GetSocket()->IsConnected() )
    {
      pStreams[sNum]->UnLock();
      pMutex.UnLock();
      return Status();
    }

    //--------------------------------------------------------------------------
    // Query the environment
    //--------------------------------------------------------------------------
    Env *env = DefaultEnv::GetEnv();
    int retryCount = DefaultConnectionRetry;
    env->GetInt( "ConnectionRetry", retryCount );
    int window = DefaultConnectionWindow;
    env->GetInt( "ConnectionWindow", window );
    int timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );

    Log *log = Utils::GetDefaultLog();
    log->Debug( PostMasterMsg, "[%s #%d] Stream fault reported. "
                               "Attempting reconnection, retry %d times, "
                               "connection window is %d seconds.",
                               pUrl.GetHostId().c_str(), sNum,
                               retryCount, window );

    Status sc;
    bool   connectionOk = true;


    //--------------------------------------------------------------------------
    // Retry the connection
    //--------------------------------------------------------------------------
    for( int trialNo = 0; trialNo < retryCount; ++trialNo )
    {
      time_t beforeConn = ::time(0);
      connectionOk = true;

      //------------------------------------------------------------------------
      // Try to reconnect
      //------------------------------------------------------------------------
      sc = pStreams[sNum]->GetSocket()->Connect( pUrl, window );
      if( sc.status != stOK )
      {
        log->Debug( PostMasterMsg, "[%s #%d] Failed to connect",
                                   pUrl.GetHostId().c_str(), sNum );
        connectionOk = false;
        ConnSleep( beforeConn, ::time(0), window,
                   pUrl.GetHostId().c_str(), sNum );
        continue;
      }

      log->Debug( PostMasterMsg, "[%s #%d] Connection successful: %s",
                                 pUrl.GetHostId().c_str(), sNum,
                                 pStreams[sNum]->GetSocket()->GetName().c_str() );

      //------------------------------------------------------------------------
      // Do the handshake
      //------------------------------------------------------------------------
      sc = pTransport->HandShake( pStreams[sNum]->GetSocket(), pUrl,
                                  sNum, pData );
      if( sc.status != stOK )
      {
        log->Error( PostMasterMsg, "[%s #%d] Failed to negotiate a link",
                                   pUrl.GetHostId().c_str(), sNum );
        pStreams[sNum]->GetSocket()->Disconnect();
        connectionOk = false;
        ConnSleep( beforeConn, ::time(0), window, pUrl.GetHostId().c_str(),
                   sNum );
        continue;
      }

      //------------------------------------------------------------------------
      // If we managed to get hear it means that we have successfully
      // reestablished the connection
      //------------------------------------------------------------------------
      break;
    }

    //--------------------------------------------------------------------------
    // Check if the connection have been established, and, if so, start
    // listening for the socket events
    //--------------------------------------------------------------------------
    if( !connectionOk )
    {
      sc.status = stFatal;
      return sc;
    }

    sc            = Status();
    bool pollerOk = true;

    //--------------------------------------------------------------------------
    // If everything went OK then add the stream to the poller
    //--------------------------------------------------------------------------
    if( pPoller->AddSocket( pStreams[sNum]->GetSocket(), pStreams[sNum] ) )
    {
      //------------------------------------------------------------------------
      // Enable read notifications
      //------------------------------------------------------------------------
      if( !pPoller->EnableReadNotification( pStreams[sNum]->GetSocket(),
                                            true, timeoutResolution ) )
      {
        log->Error( PostMasterMsg, "[%s #%d] Unable to listen to read events "
                                   "on the stream",
                                   pUrl.GetHostId().c_str(), sNum );
        sc = Status( stError, errPollerError );
        pollerOk = false;
      }

      //------------------------------------------------------------------------
      // Enable write notifications
      //------------------------------------------------------------------------
      if( !pPoller->EnableWriteNotification( pStreams[sNum]->GetSocket(),
                                             true, timeoutResolution ) )
      {
        log->Error( PostMasterMsg, "[%s #%d] Unable to listen to write events "
                                   "on the stream",
                                   pUrl.GetHostId().c_str(), sNum );
        sc = Status( stError, errPollerError );
        pollerOk = false;
      }
    }
    //------------------------------------------------------------------------
    // Failed to add to the poller
    //------------------------------------------------------------------------
    else
    {
      log->Error( PostMasterMsg, "[%s #%d] Unable to register the stream "
                                 " with the poller",
                                 pUrl.GetHostId().c_str(), sNum );
      sc =  Status( stError, errPollerError );
      pollerOk = false;
    }

    //--------------------------------------------------------------------------
    // We had problems adding all the streams to the poller, disconnect
    //--------------------------------------------------------------------------
    if( !pollerOk )
    {
      pPoller->RemoveSocket( pStreams[sNum]->GetSocket() );
      pStreams[sNum]->GetSocket()->Disconnect();
    }

    pStreams[sNum]->UnLock();
    pMutex.UnLock();
    return sc;
  }

  //----------------------------------------------------------------------------
  // Print sleep info
  //----------------------------------------------------------------------------
  void Channel::ConnSleep( time_t start, time_t now, time_t window,
                           const char *hostId, uint16_t streamNum )
  {
    time_t elapsed = now - start;
    if( elapsed < window )
    {
      Log *log = Utils::GetDefaultLog();
      log->Debug( PostMasterMsg, "[%s #%d] Sleeping %d seconds",
                                 hostId, streamNum, window-elapsed );
      ::sleep( window - elapsed );
    }
  }
}
