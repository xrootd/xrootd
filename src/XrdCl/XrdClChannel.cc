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
    delete pStream;
    pTransport->FinalizeChannel( pChannelData );
  }

  //----------------------------------------------------------------------------
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  XRootDStatus Channel::Send( Message              *msg,
                              MsgHandler   *handler,
                              bool                  stateful,
                              time_t                expires )

  {
    return pStream->Send( msg, handler, stateful, expires );
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

  //----------------------------------------------------------------------------
  // Force reconnect
  //----------------------------------------------------------------------------
  Status Channel::ForceReconnect()
  {
    pStream->ForceConnect();
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
    if( query < 2000 )
      return pTransport->Query( query, result, pChannelData );
    return pStream->Query( query, result );
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
