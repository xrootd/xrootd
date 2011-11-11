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
      StatusHandler( XrdClient::Message *msg ): pMsg( msg ) {}

      //------------------------------------------------------------------------
      // Handle the status information
      //------------------------------------------------------------------------
      void HandleStatus( const XrdClient::Message *message,
                         XrdClient::Status         status )
      {
        if( pMsg == message )
          pStatus = status;
        pCondVar.Broadcast();
      }

      //------------------------------------------------------------------------
      // Wait for the status to be ready
      //------------------------------------------------------------------------
      XrdClient:: Status WaitForStatus()
      {
        pCondVar.Wait();
        return pStatus;
      }
      
    private:
      XrdSysCondVar       pCondVar;
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
  Status Channel::HandleStreamFault( uint16_t streamNum )
  {
    //--------------------------------------------------------------------------
    // Query the environment
    //--------------------------------------------------------------------------
    Env *env = DefaultEnv::GetEnv();
    int retryCount = DefaultConnectionRetry;
    env->GetInt( "ConnectionRetry", retryCount );
    int timeout = DefaultConnectionTimeout;
    env->GetInt( "ConnectionTimeout", timeout );

    Log *log = Utils::GetDefaultLog();
    log->Debug( PostMasterMsg, "[%s #%d] Stream fault reported. "
                               "Attempting reconnection, retry %d times, "
                               "wait %d seconds.",
                               pUrl.GetHostId().c_str(), streamNum,
                               retryCount, timeout );

    Status sc;
    bool connectionOk = true;

    //--------------------------------------------------------------------------
    // Lock the channel and grab the locks of all the streams
    //--------------------------------------------------------------------------
    pMutex.Lock();
    for( int i = 0; i < pStreams.size(); ++i )
      pStreams[i]->Lock();

    //--------------------------------------------------------------------------
    // Retry the connection
    //--------------------------------------------------------------------------
    for( int trialNo = 0; trialNo < retryCount; ++trialNo )
    {
      //------------------------------------------------------------------------
      // Make sure that all the streams are in good shape
      //------------------------------------------------------------------------
      time_t beforeConn = ::time(0);
      connectionOk = true;
      for( int i = 0; i < pStreams.size(); ++i )
      {
        if( !pStreams[i]->GetSocket()->IsConnected() )
        {
          //--------------------------------------------------------------------
          // Cleanup and try to reconnect
          //--------------------------------------------------------------------
          pPoller->RemoveSocket( pStreams[i]->GetSocket() );
          sc = pStreams[i]->GetSocket()->Connect( pUrl, timeout );
          if( sc.status != stOK )
          {
            log->Debug( PostMasterMsg, "[%s #%d] Failed to connect",
                                       pUrl.GetHostId().c_str(), i );
            connectionOk = false;
            break;
          }

          log->Debug( PostMasterMsg, "[%s #%d] Connection successful: %s",
                                     pUrl.GetHostId().c_str(), i,
                                     pStreams[i]->GetSocket()->GetName().c_str() );

          //--------------------------------------------------------------------
          // Do the handshake
          //--------------------------------------------------------------------
          sc = pTransport->HandShake( pStreams[i]->GetSocket(), pUrl, i, pData );
          if( sc.status != stOK )
          {
            log->Error( PostMasterMsg, "[%s #%d] Failed to negotiate a link",
                                       pUrl.GetHostId().c_str(), i );
            pStreams[i]->GetSocket()->Disconnect();
            connectionOk = false;
            break;
          }
        }
      }

      //------------------------------------------------------------------------
      // Sleep before next try if needed
      //------------------------------------------------------------------------
      if( !connectionOk )
      {
        if( trialNo != retryCount-1 )
        {
          int sleepTime = timeout - (::time(0) - beforeConn);
          if( sleepTime < 0 ) sleepTime = 0;

          log->Debug( PostMasterMsg, "[%s] Sleeping %d seconds before retry",
                                     pUrl.GetHostId().c_str(), sleepTime );
          ::sleep( sleepTime );
        }
      }
      else
        break;
    }

    //--------------------------------------------------------------------------
    // We were unable to connect
    //--------------------------------------------------------------------------
    if( !connectionOk )
    {
      for( int i = 0; i < pStreams.size(); ++i )
        pStreams[i]->UnLock();
      pMutex.UnLock();
      return sc;
    }

    //--------------------------------------------------------------------------
    // If everything went OK then add the streams to the poller
    //--------------------------------------------------------------------------
    bool pollerOk = true;
    for( int i = 0; i < pStreams.size(); ++i )
    {
      //------------------------------------------------------------------------
      // Successfully added to poller
      //------------------------------------------------------------------------
      if( pPoller->AddSocket( pStreams[i]->GetSocket(), pStreams[i] ) )
      {

        //----------------------------------------------------------------------
        // Enable read notifications
        //----------------------------------------------------------------------
        if( !pPoller->EnableReadNotification( pStreams[i]->GetSocket(),
                                              true, 15 ) )
        {
          log->Error( PostMasterMsg, "[%s #%d] Unable to listen to read events "
                                     "on the stream",
                                     pUrl.GetHostId().c_str(), i );
          sc = Status( stError, errPollerError );
          pollerOk = false;
          break;
        }

        //----------------------------------------------------------------------
        // Enable write notifications
        //----------------------------------------------------------------------
        if( !pPoller->EnableWriteNotification( pStreams[i]->GetSocket(),
                                              true, 15 ) )
        {
          log->Error( PostMasterMsg, "[%s #%d] Unable to listen to write events "
                                     "on the stream",
                                     pUrl.GetHostId().c_str(), i );
          sc = Status( stError, errPollerError );
          pollerOk = false;
          break;
        }
      }

      //----------------------------------------------------------------------
      // Failed to add to the poller
      //----------------------------------------------------------------------
      else
      {
        log->Error( PostMasterMsg, "[%s #%d] Unable to register the stream "
                                   " with the poller",
                                   pUrl.GetHostId().c_str(), i );
        sc =  Status( stError, errPollerError );
        pollerOk = false;
        break;
      }
    }

    //--------------------------------------------------------------------------
    // We had problems adding all the streams to the poller, disconnect
    //--------------------------------------------------------------------------
    if( !pollerOk )
    {
      for( int i = 0; i < pStreams.size(); ++i )
      {
        pPoller->RemoveSocket( pStreams[i]->GetSocket() );
        pStreams[i]->GetSocket()->Disconnect();
      }
    }

    for( int i = 0; i < pStreams.size(); ++i )
      pStreams[i]->UnLock();
    pMutex.UnLock();

    return sc;
  }
}
