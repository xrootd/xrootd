//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClChannel.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUglyHacks.hh"

#include <ctime>

namespace
{
  //----------------------------------------------------------------------------
  // Filter handler
  //----------------------------------------------------------------------------
  class FilterHandler: public XrdCl::IncomingMsgHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      FilterHandler( XrdCl::MessageFilter *filter ):
        pSem( new XrdCl::Semaphore(0) ), pFilter( filter ), pMsg( 0 )
      {
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~FilterHandler()
      {
        delete pSem;
      }

      //------------------------------------------------------------------------
      // Message handler
      //------------------------------------------------------------------------
      virtual uint16_t Examine( XrdCl::Message *msg )
      {
        if( pFilter->Filter( msg ) )
          return Take | RemoveHandler;
        return Ignore;
      }

      virtual void Process( XrdCl::Message *msg )
      {
        pMsg = msg;
        pSem->Post();
      }

      //------------------------------------------------------------------------
      // Handle a fault
      //------------------------------------------------------------------------
      virtual uint8_t OnStreamEvent( StreamEvent   event,
                                     uint16_t      streamNum,
                                     XrdCl::Status status )
      {
        if( event == Ready )
          return 0;
        pStatus = status;
        pSem->Post();
        return RemoveHandler;
      }

      //------------------------------------------------------------------------
      // Wait for a status of the message
      //------------------------------------------------------------------------
      XrdCl::Status WaitForStatus()
      {
        pSem->Wait();
        return pStatus;
      }

      //------------------------------------------------------------------------
      // Wait for the arrival of the message
      //------------------------------------------------------------------------
      XrdCl::Message *GetMessage()
      {
        return pMsg;
      }

    private:
      FilterHandler(const FilterHandler &other);
      FilterHandler &operator = (const FilterHandler &other);

      XrdCl::Semaphore     *pSem;
      XrdCl::MessageFilter *pFilter;
      XrdCl::Message       *pMsg;
      XrdCl::Status         pStatus;
  };

  //----------------------------------------------------------------------------
  // Status handler
  //----------------------------------------------------------------------------
  class StatusHandler: public XrdCl::OutgoingMsgHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StatusHandler( XrdCl::Message *msg ):
        pSem( new XrdCl::Semaphore(0) ),
        pMsg( msg ) {}

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~StatusHandler()
      {
        delete pSem;
      }

      //------------------------------------------------------------------------
      // Handle the status information
      //------------------------------------------------------------------------
      void OnStatusReady( const XrdCl::Message *message,
                          XrdCl::Status         status )
      {
        if( pMsg == message )
          pStatus = status;
        pSem->Post();
      }

      //------------------------------------------------------------------------
      // Wait for the status to be ready
      //------------------------------------------------------------------------
      XrdCl::Status WaitForStatus()
      {
        pSem->Wait();
        return pStatus;
      }
      
    private:
      StatusHandler(const StatusHandler &other);
      StatusHandler &operator = (const StatusHandler &other);

      XrdCl::Semaphore *pSem;
      XrdCl::Status     pStatus;
      XrdCl::Message   *pMsg;
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
                    TaskManager      *taskManager,
                    JobManager       *jobManager ):
    pUrl( url.GetHostId() ),
    pPoller( poller ),
    pTransport( transport ),
    pTaskManager( taskManager ),
    pTickGenerator( 0 ),
    pJobManager( jobManager )
  {
    Env *env = DefaultEnv::GetEnv();
    Log *log = DefaultEnv::GetLog();

    int  timeoutResolution = DefaultTimeoutResolution;
    env->GetInt( "TimeoutResolution", timeoutResolution );

    pTransport->InitializeChannel( pChannelData );
    uint16_t numStreams = transport->StreamNumber( pChannelData );
    log->Debug( PostMasterMsg, "Creating new channel to: %s %d stream(s)",
                                url.GetHostId().c_str(), numStreams );

    pUrl.SetParams( url.GetParams() );

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
      pStreams[i]->SetJobManager( jobManager );
      pStreams[i]->SetChannelData( &pChannelData );
      pStreams[i]->Initialize();
    }

    //--------------------------------------------------------------------------
    // Register the task generating timeout events
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
  Status Channel::Send( Message *msg, bool stateful, time_t expires )
  {
    StatusHandler sh( msg );
    Status sc = Send( msg, &sh, stateful, expires );
    if( !sc.IsOK() )
      return sc;
    sc = sh.WaitForStatus();
    return sc;
  }

  //----------------------------------------------------------------------------
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  Status Channel::Send( Message              *msg,
                        OutgoingMsgHandler   *handler,
                        bool                  stateful,
                        time_t                expires )

  {
    PathID path = pTransport->Multiplex( msg, pChannelData );
    return pStreams[path.up]->Send( msg, handler, stateful, expires );
  }

  //----------------------------------------------------------------------------
  // Synchronously receive a message - blocks until a message matching
  //----------------------------------------------------------------------------
  Status Channel::Receive( Message       *&msg,
                           MessageFilter  *filter,
                           time_t          expires )
  {
    FilterHandler fh( filter );
    Status sc = Receive( &fh, expires );
    if( !sc.IsOK() )
      return sc;

    sc = fh.WaitForStatus();
    if( sc.IsOK() )
      msg = fh.GetMessage();
    return sc;
  }

  //----------------------------------------------------------------------------
  // Listen to incoming messages
  //----------------------------------------------------------------------------
  Status Channel::Receive( IncomingMsgHandler *handler, time_t expires )
  {
    pIncoming.AddMessageHandler( handler, expires );
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

  //----------------------------------------------------------------------------
  // Register channel event handler
  //----------------------------------------------------------------------------
  void Channel::RegisterEventHandler( ChannelEventHandler *handler )
  {
    std::vector<Stream *>::iterator it;
    for( it = pStreams.begin(); it != pStreams.end(); ++it )
      (*it)->RegisterEventHandler( handler );
  }

  //------------------------------------------------------------------------
  // Remove a channel event handler
  //------------------------------------------------------------------------
  void Channel::RemoveEventHandler( ChannelEventHandler *handler )
  {
    std::vector<Stream *>::iterator it;
    for( it = pStreams.begin(); it != pStreams.end(); ++it )
      (*it)->RemoveEventHandler( handler );
  }
}
