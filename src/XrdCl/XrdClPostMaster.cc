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

#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClPollerFactory.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClTransportManager.hh"
#include "XrdCl/XrdClChannel.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  PostMaster::PostMaster():
    pPoller( 0 ), pInitialized( false )
  {
    Env *env = DefaultEnv::GetEnv();
    int workerThreads = DefaultWorkerThreads;
    env->GetInt( "WorkerThreads", workerThreads );

    pTaskManager = new TaskManager();
    pJobManager  = new JobManager(workerThreads);
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  PostMaster::~PostMaster()
  {
    delete pPoller;
    delete pTaskManager;
    delete pJobManager;
  }

  //----------------------------------------------------------------------------
  // Initializer
  //----------------------------------------------------------------------------
  bool PostMaster::Initialize()
  {
    Env *env = DefaultEnv::GetEnv();
    std::string pollerPref = DefaultPollerPreference;
    env->GetString( "PollerPreference", pollerPref );

    pPoller = PollerFactory::CreatePoller( pollerPref );

    if( !pPoller )
      return false;

    bool st = pPoller->Initialize();

    if( !st )
    {
      delete pPoller;
      return false;
    }

    pJobManager->Initialize();
    pInitialized = true;
    return true;
  }

  //----------------------------------------------------------------------------
  // Finalizer
  //----------------------------------------------------------------------------
  bool PostMaster::Finalize()
  {
    //--------------------------------------------------------------------------
    // Clean up the channels
    //--------------------------------------------------------------------------
    if( !pInitialized )
      return true;

    pInitialized = false;
    pJobManager->Finalize();
    ChannelMap::iterator it;

    for( it = pChannelMap.begin(); it != pChannelMap.end(); ++it )
      delete it->second;

    pChannelMap.clear();
    return pPoller->Finalize();
  }

  //----------------------------------------------------------------------------
  // Start the post master
  //----------------------------------------------------------------------------
  bool PostMaster::Start()
  {
    if( !pInitialized )
      return false;

    if( !pPoller->Start() )
      return false;

    if( !pTaskManager->Start() )
    {
      pPoller->Stop();
      return false;
    }

    if( !pJobManager->Start() )
    {
      pPoller->Stop();
      pTaskManager->Stop();
      return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // Stop the postmaster
  //----------------------------------------------------------------------------
  bool PostMaster::Stop()
  {
    if( !pInitialized )
      return true;

    if( !pJobManager->Stop() )
      return false;
    if( !pTaskManager->Stop() )
      return false;
    if( !pPoller->Stop() )
      return false;
    return true;
  }

  //----------------------------------------------------------------------------
  // Reinitialize after fork
  //----------------------------------------------------------------------------
  bool PostMaster::Reinitialize()
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Send a message synchronously
  //----------------------------------------------------------------------------
  Status PostMaster::Send( const URL &url,
                           Message   *msg,
                           bool       stateful,
                           time_t     expires )
  {
    Channel *channel = GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    return channel->Send( msg, stateful, expires );
  }

  //----------------------------------------------------------------------------
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  Status PostMaster::Send( const URL            &url,
                           Message              *msg,
                           OutgoingMsgHandler   *handler,
                           bool                  stateful,
                           time_t                expires )
  {
    Channel *channel = GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    return channel->Send( msg, handler, stateful, expires );
  }

  Status PostMaster::Redirect( const URL          &url,
                               Message            *msg,
                               IncomingMsgHandler *inHandler )
  {
    RedirectorRegistry &registry  = RedirectorRegistry::Instance();
    VirtualRedirector *redirector = registry.Get( url );
    if( !redirector )
      return Status( stError, errInvalidOp );
    return redirector->HandleRequest( msg, inHandler );
  }

  //----------------------------------------------------------------------------
  // Synchronously receive a message
  //----------------------------------------------------------------------------
  Status PostMaster::Receive( const URL      &url,
                              Message       *&msg,
                              MessageFilter  *filter,
                              time_t          expires )
  {
    Channel *channel = GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    return channel->Receive( msg, filter, expires );
  }

  //----------------------------------------------------------------------------
  // Listen to incoming messages
  //----------------------------------------------------------------------------
  Status PostMaster::Receive( const URL          &url,
                              IncomingMsgHandler *handler,
                              time_t              expires )
  {
    Channel *channel = GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    return channel->Receive( handler, expires );
  }

  //----------------------------------------------------------------------------
  // Query the transport handler
  //----------------------------------------------------------------------------
  Status PostMaster::QueryTransport( const URL &url,
                                     uint16_t   query,
                                     AnyObject &result )
  {
    Channel *channel = GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    return channel->QueryTransport( query, result );
  }

  //----------------------------------------------------------------------------
  // Register channel event handler
  //----------------------------------------------------------------------------
  Status PostMaster::RegisterEventHandler( const URL           &url,
                                           ChannelEventHandler *handler )
  {
    Channel *channel = GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    channel->RegisterEventHandler( handler );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Remove a channel event handler
  //----------------------------------------------------------------------------
  Status PostMaster::RemoveEventHandler( const URL           &url,
                                       ChannelEventHandler *handler )
  {
    Channel *channel = GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    channel->RemoveEventHandler( handler );
    return Status();
  }

  //------------------------------------------------------------------------
  // Shut down a channel
  //------------------------------------------------------------------------
  Status PostMaster::ForceDisconnect( const URL &url )
  {
    XrdSysMutexHelper scopedLock( pChannelMapMutex );
    ChannelMap::iterator it = pChannelMap.find( url.GetHostId() );

    if( it == pChannelMap.end() )
      return Status( stError, errInvalidOp );

    it->second->ForceDisconnect();
    delete it->second;
    pChannelMap.erase( it );

    return Status();
  }

  //----------------------------------------------------------------------------
  // Get the channel
  //----------------------------------------------------------------------------
  Channel *PostMaster::GetChannel( const URL &url )
  {
    XrdSysMutexHelper scopedLock( pChannelMapMutex );
    Channel *channel = 0;
    ChannelMap::iterator it = pChannelMap.find( url.GetHostId() );

    if( it == pChannelMap.end() )
    {
      TransportManager *trManager = DefaultEnv::GetTransportManager();
      TransportHandler *trHandler = trManager->GetHandler( url.GetProtocol() );

      if( !trHandler )
      {
        Log *log = DefaultEnv::GetLog();
        log->Error( PostMasterMsg, "Unable to get transport handler for %s "
                    "protocol", url.GetProtocol().c_str() );
        return 0;
      }

      channel = new Channel( url, pPoller, trHandler, pTaskManager, pJobManager );
      pChannelMap[url.GetHostId()] = channel;
    }
    else
      channel = it->second;
    return channel;
  }
}
