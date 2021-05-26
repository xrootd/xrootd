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
#include "XrdCl/XrdClRedirectorRegistry.hh"
#include "XrdCl/XrdClXRootDTransport.hh"

#include "XrdSys/XrdSysPthread.hh"

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
        pSem( new XrdSysSemaphore(0) ), pFilter( filter ), pMsg( 0 )
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

      //------------------------------------------------------------------------
      //! Reexamine the incoming message, and decide on the action to be taken
      //------------------------------------------------------------------------
      virtual uint16_t InspectStatusRsp( XrdCl::Message *msg )
      {
        return 0;
      }

      virtual void Process( XrdCl::Message *msg )
      {
        pMsg = msg;
        pSem->Post();
      }

      //------------------------------------------------------------------------
      // Handle a fault
      //------------------------------------------------------------------------
      virtual uint8_t OnStreamEvent( StreamEvent         event,
                                     XrdCl::XRootDStatus status )
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
      XrdCl::XRootDStatus WaitForStatus()
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

      //------------------------------------------------------------------------
      // Get underlying message filter sid
      //------------------------------------------------------------------------
      uint16_t GetSid() const
      {
	if (pFilter)
	  return pFilter->GetSid();

	return 0;
      }

    private:
      FilterHandler(const FilterHandler &other);
      FilterHandler &operator = (const FilterHandler &other);

      XrdSysSemaphore      *pSem;
      XrdCl::MessageFilter *pFilter;
      XrdCl::Message       *pMsg;
      XrdCl::XRootDStatus   pStatus;
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
        pSem( new XrdSysSemaphore(0) ),
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
                          XrdCl::XRootDStatus   status )
      {
        if( pMsg == message )
          pStatus = status;
        pSem->Post();
      }

      //------------------------------------------------------------------------
      // Wait for the status to be ready
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus WaitForStatus()
      {
        pSem->Wait();
        return pStatus;
      }
      
    private:
      StatusHandler(const StatusHandler &other);
      StatusHandler &operator = (const StatusHandler &other);

      XrdSysSemaphore     *pSem;
      XrdCl::XRootDStatus  pStatus;
      XrdCl::Message      *pMsg;
  };

}

namespace XrdCl
{
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
        XrdSysMutexHelper lck( pMtx );
        if( !pChannel ) return 0;

        using namespace XrdCl;
        pChannel->Tick( now );

        Env *env = DefaultEnv::GetEnv();
        int timeoutResolution = DefaultTimeoutResolution;
        env->GetInt( "TimeoutResolution", timeoutResolution );
        return now+timeoutResolution;
      }

      void Invalidate()
      {
        XrdSysMutexHelper lck( pMtx );
        pChannel = 0;
      }

    private:
      XrdCl::Channel *pChannel;
      XrdSysMutex     pMtx;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  Channel::Channel( const URL        &url,
                    Poller           *poller,
                    TransportHandler *transport,
                    TaskManager      *taskManager,
                    JobManager       *jobManager,
                    const URL        &prefurl ):
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

    pTransport->InitializeChannel( url, pChannelData );
    log->Debug( PostMasterMsg, "Creating new channel to: %s",
                                url.GetChannelId().c_str() );

    pUrl.SetParams( url.GetParams() );
    pUrl.SetProtocol( url.GetProtocol() );

    //--------------------------------------------------------------------------
    // Create the stream
    //--------------------------------------------------------------------------
    pStream = new Stream( &pUrl, prefurl );
    pStream->SetTransport( transport );
    pStream->SetPoller( poller );
    pStream->SetIncomingQueue( &pIncoming );
    pStream->SetTaskManager( taskManager );
    pStream->SetJobManager( jobManager );
    pStream->SetChannelData( &pChannelData );
    pStream->Initialize();

    //--------------------------------------------------------------------------
    // Register the task generating timeout events
    //--------------------------------------------------------------------------
    pTickGenerator = new TickGeneratorTask( this, pUrl.GetChannelId() );
    pTaskManager->RegisterTask( pTickGenerator, ::time(0)+timeoutResolution );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Channel::~Channel()
  {
    pTickGenerator->Invalidate();
    pTaskManager->UnregisterTask( pTickGenerator );
    delete pStream;
    pTransport->FinalizeChannel( pChannelData );
  }

  //----------------------------------------------------------------------------
  // Send a message synchronously
  //----------------------------------------------------------------------------
  XRootDStatus Channel::Send( Message *msg, bool stateful, time_t expires )
  {
    StatusHandler sh( msg );
    XRootDStatus sc = Send( msg, &sh, stateful, expires );
    if( !sc.IsOK() )
      return sc;
    sc = sh.WaitForStatus();
    return sc;
  }

  //----------------------------------------------------------------------------
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  XRootDStatus Channel::Send( Message              *msg,
                              OutgoingMsgHandler   *handler,
                              bool                  stateful,
                              time_t                expires )

  {
    return pStream->Send( msg, handler, stateful, expires );
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
    pStream->Tick( now );
  }

  //----------------------------------------------------------------------------
  // Force disconnect of all streams
  //----------------------------------------------------------------------------
  Status Channel::ForceDisconnect()
  {
    //--------------------------------------------------------------------------
    // Disconnect and recreate the streams
    //--------------------------------------------------------------------------
    pStream->ForceError( Status( stError, errOperationInterrupted ) );

    return Status();
  }

  //------------------------------------------------------------------------
  // Get the number of connected data streams
  //------------------------------------------------------------------------
  uint16_t Channel::NbConnectedStrm()
  {
    return XRootDTransport::NbConnectedStrm( pChannelData );
  }

  //------------------------------------------------------------------------
  // Set the on-connect handler for data streams
  //------------------------------------------------------------------------
  void Channel::SetOnDataConnectHandler( std::shared_ptr<Job> &onConnJob )
  {
    pStream->SetOnDataConnectHandler( onConnJob );
  }

  //------------------------------------------------------------------------
  // Check if channel can be collapsed using given URL
  //------------------------------------------------------------------------
  bool Channel::CanCollapse( const URL &url )
  {
    return pStream->CanCollapse( url );
  }

  //------------------------------------------------------------------------
  // Decrement file object instance count bound to this channel
  //------------------------------------------------------------------------
  void Channel::DecFileInstCnt()
  {
    pTransport->DecFileInstCnt( pChannelData );
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
    pStream->RegisterEventHandler( handler );
  }

  //------------------------------------------------------------------------
  // Remove a channel event handler
  //------------------------------------------------------------------------
  void Channel::RemoveEventHandler( ChannelEventHandler *handler )
  {
    pStream->RemoveEventHandler( handler );
  }
}
