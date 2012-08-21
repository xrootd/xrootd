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

#include "XrdCl/XrdClChannel.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"

#include <ctime>

namespace
{
  //----------------------------------------------------------------------------
  // Filter handler
  //----------------------------------------------------------------------------
  class FilterHandler: public XrdCl::MessageHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      FilterHandler( XrdCl::MessageFilter *filter ):
        pSem( 0 ), pFilter( filter ), pMsg( 0 )
      {
      }

      //------------------------------------------------------------------------
      // Message handler
      //------------------------------------------------------------------------
      virtual uint8_t HandleMessage( XrdCl::Message *msg )
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
      virtual void HandleFault( XrdCl::Status status )
      {
        pStatus = status;
        pSem.Post();
      }

      //------------------------------------------------------------------------
      // Wait for a status of the message
      //------------------------------------------------------------------------
      XrdCl::Status WaitForStatus()
      {
        pSem.Wait();
        return pStatus;
      }

      //------------------------------------------------------------------------
      // Wait for the arraival of the message
      //------------------------------------------------------------------------
      XrdCl::Message *GetMessage()
      {
        return pMsg;
      }

    private:
      XrdSysSemaphore           pSem;
      XrdCl::MessageFilter *pFilter;
      XrdCl::Message       *pMsg;
      XrdCl::Status         pStatus;
  };

  //----------------------------------------------------------------------------
  // Status handler
  //----------------------------------------------------------------------------
  class StatusHandler: public XrdCl::MessageStatusHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StatusHandler( XrdCl::Message *msg ): pSem( 0 ), pMsg( msg ) {}

      //------------------------------------------------------------------------
      // Handle the status information
      //------------------------------------------------------------------------
      void HandleStatus( const XrdCl::Message *message,
                         XrdCl::Status         status )
      {
        if( pMsg == message )
          pStatus = status;
        pSem.Post();
      }

      //------------------------------------------------------------------------
      // Wait for the status to be ready
      //------------------------------------------------------------------------
      XrdCl:: Status WaitForStatus()
      {
        pSem.Wait();
        return pStatus;
      }
      
    private:
      XrdSysSemaphore     pSem;
      XrdCl::Status   pStatus;
      XrdCl::Message *pMsg;
  };

  class TickGeneratorTask: public XrdCl::Task
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      TickGeneratorTask( XrdCl::Channel *channel, const std::string &hostId ):
        pChannel( channel )
      {
        std::string name = "TickGeneratorTask for: ";
        name += hostId;
        SetName( name );
      }

      //------------------------------------------------------------------------
      // Run the task
      //------------------------------------------------------------------------
      time_t Run( time_t now )
      {
        using namespace XrdCl;
        pChannel->Tick( now );

        Env *env = DefaultEnv::GetEnv();
        int timeoutResolution = DefaultTimeoutResolution;
        env->GetInt( "TimeoutResolution", timeoutResolution );
        return now+timeoutResolution;
      }
    private:
      XrdCl::Channel *pChannel;
  };
}

namespace XrdCl
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
    Log *log = DefaultEnv::GetLog();

    int  timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );

    pTransport->InitializeChannel( pChannelData );
    uint16_t numStreams = transport->StreamNumber( pChannelData );
    log->Debug( PostMasterMsg, "Creating new channel to: %s %d stream(s)",
                                url.GetHostId().c_str(), numStreams );

    //--------------------------------------------------------------------------
    // Create the streams
    //--------------------------------------------------------------------------
    pStreams.resize( numStreams );
    for( uint16_t i = 0; i < numStreams; ++i )
    {
      pStreams[i] = new Stream( &pUrl, i );
      pStreams[i]->SetTransport( transport );
      pStreams[i]->SetPoller( poller );
      pStreams[i]->SetIncomingQueue( &pIncoming );
      pStreams[i]->SetTaskManager( taskManager );
      pStreams[i]->SetChannelData( &pChannelData );
      pStreams[i]->Initialize();
    }

    //--------------------------------------------------------------------------
    // Register the task generating timout events
    //--------------------------------------------------------------------------
    pTickGenerator = new TickGeneratorTask( this, pUrl.GetHostId() );
    pTaskManager->RegisterTask( pTickGenerator, ::time(0)+timeoutResolution );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Channel::~Channel()
  {
    pTaskManager->UnregisterTask( pTickGenerator );
    for( uint32_t i = 0; i < pStreams.size(); ++i )
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
    PathID path = pTransport->Multiplex( msg, pChannelData );
    return pStreams[path.up]->Send( msg, statusHandler, timeout );
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
