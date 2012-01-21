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
      // Handle a fault
      //------------------------------------------------------------------------
      virtual void HandleFault( XrdClient::Status status )
      {
        pStatus = status;
        pSem.Post();
      }

      //------------------------------------------------------------------------
      // Wait for a status of the message
      //------------------------------------------------------------------------
      XrdClient::Status WaitForStatus()
      {
        pSem.Wait();
        return pStatus;
      }

      //------------------------------------------------------------------------
      // Wait for the arraival of the message
      //------------------------------------------------------------------------
      XrdClient::Message *GetMessage()
      {
        return pMsg;
      }

    private:
      XrdSysSemaphore           pSem;
      XrdClient::MessageFilter *pFilter;
      XrdClient::Message       *pMsg;
      XrdClient::Status         pStatus;
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

  class TickGeneratorTask: public XrdClient::Task
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      TickGeneratorTask( XrdClient::Channel *channel ):
        pChannel( channel ) {}

      //------------------------------------------------------------------------
      // Run the task
      //------------------------------------------------------------------------
      time_t Run( time_t now )
      {
        using namespace XrdClient;
        pChannel->Tick( now );

        Env *env = DefaultEnv::GetEnv();
        int timeoutResolution = DefaultTimeoutResolution;
        env->GetInt( "TimeoutResolution", timeoutResolution );
        return now+timeoutResolution;
      }

    private:
      XrdClient::Channel *pChannel;
  };
}

namespace XrdClient
{

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  Channel::Channel( const URL        &url,
                    Poller           *poller,
                    TransportHandler *transport,
                    TaskManager      *taskManager ):
    pUrl( url.GetHostId() ),
    pPoller( poller ),
    pTransport( transport ),
    pTaskManager( taskManager ),
    pTickGenerator( 0 )
  {
    Env *env = DefaultEnv::GetEnv();
    Log *log = Utils::GetDefaultLog();

    int  numStreams = DefaultStreamsPerChannel;
    env->GetInt( "StreamsPerChannel", numStreams );

    int  timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );

    log->Debug( PostMasterMsg, "Creating new channel to: %s %d stream(s)",
                                url.GetHostId().c_str(), numStreams );

    pTransport->InitializeChannel( pChannelData );

    //--------------------------------------------------------------------------
    // Create the streams
    //--------------------------------------------------------------------------
    pStreams.resize( numStreams );
    for( int i = 0; i < numStreams; ++i )
    {
      pStreams[i] = new Stream( &pUrl, i );
      pStreams[i]->SetTransport( transport );
      pStreams[i]->SetPoller( poller );
      pStreams[i]->SetIncomingQueue( &pIncoming );
      pStreams[i]->SetTaskManager( taskManager );
      pStreams[i]->SetChannelData( &pChannelData );
    }

    //--------------------------------------------------------------------------
    // Register the task generating timout events
    //--------------------------------------------------------------------------
    pTickGenerator = new TickGeneratorTask( this );
    pTaskManager->RegisterTask( pTickGenerator, ::time(0)+timeoutResolution );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Channel::~Channel()
  {
    pTaskManager->UnregisterTask( pTickGenerator );
    delete pTickGenerator;
    for( int i = 0; i < pStreams.size(); ++i )
      delete pStreams[i];
    pTransport->FinalizeChannel( pChannelData );
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
    uint16_t stream = pTransport->Multiplex( msg, pChannelData );
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

    sc = fh.WaitForStatus();
    if( sc.IsOK() )
      msg = fh.GetMessage();
    return sc;
  }

  //----------------------------------------------------------------------------
  // Listen to incomming messages
  //----------------------------------------------------------------------------
  Status Channel::Receive( MessageHandler *handler, uint16_t timeout )
  {
    time_t tm = ::time(0) + timeout;
    pIncoming.AddMessageHandler( handler, tm );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Handle a time event
  //----------------------------------------------------------------------------
  void Channel::Tick( time_t now )
  {
    std::vector<Stream *>::iterator it;
    for( it = pStreams.begin(); it != pStreams.end(); ++it )
      (*it)->Tick( now );
  }

  //----------------------------------------------------------------------------
  // Query the transport handler
  //----------------------------------------------------------------------------
  Status Channel::QueryTransport( uint16_t query, AnyObject &result )
  {
    return pTransport->Query( query, result, pChannelData );
  }
}
