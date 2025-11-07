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

#include "XrdSys/XrdSysPthread.hh"

#include <unordered_set>

namespace XrdCl
{
  struct ConnErrJob : public Job
  {
    ConnErrJob( const URL &url, const XRootDStatus &status,
                std::function<void( const URL&, const XRootDStatus& )> handler) : url( url ),
                                                                                  status( status ),
                                                                                  handler( handler )
    {
    }

    void Run( void *arg )
    {
      handler( url, status );
      delete this;
    }

    URL url;
    XRootDStatus status;
    std::function<void( const URL&, const XRootDStatus& )> handler;
  };

  struct PostMasterImpl
  {
    PostMasterImpl() : pPoller( 0 ), pInitialized( false ), pRunning( false )
    {
      Env *env = DefaultEnv::GetEnv();
      int workerThreads = DefaultWorkerThreads;
      env->GetInt( "WorkerThreads", workerThreads );

      pTaskManager = new TaskManager();
      pJobManager  = new JobManager(workerThreads);
    }

    ~PostMasterImpl()
    {
      delete pPoller;
      delete pTaskManager;
      delete pJobManager;
    }

    //--------------------------------------------------------------------------
    //! Used to maintain a non-owning set of live Channels. Used by Finalize.
    //--------------------------------------------------------------------------
    void addFinalize(Channel *ch)
    {
      XrdSysMutexHelper lck( pFinalizeSetMutex );
      pFinalizeSet.insert( ch );
    }

    //--------------------------------------------------------------------------
    //! Get a channel for url, creating one if needed.
    //--------------------------------------------------------------------------
    std::shared_ptr<Channel> GetChannel( const URL &url );

    //--------------------------------------------------------------------------
    //! Used to maintain a non-owning set of live Channels. Used by Finalize.
    //--------------------------------------------------------------------------
    void removeFinalize(Channel *ch)
    {
      XrdSysMutexHelper lck( pFinalizeSetMutex );
      pFinalizeSet.erase( ch );
    }

    typedef std::map<std::string, std::shared_ptr<Channel> > ChannelMap;

    Poller               *pPoller;
    TaskManager          *pTaskManager;
    ChannelMap            pChannelMap;
    std::unordered_set<Channel*> pFinalizeSet;

    // Mutex protecting access of pChannelMap
    XrdSysMutex           pChannelMapMutex;

    // Mutex protecting access of pFinalizeSet
    XrdSysMutex           pFinalizeSetMutex;

    bool                  pInitialized;
    bool                  pRunning;
    JobManager           *pJobManager;

    XrdSysMutex           pMtx;
    std::unique_ptr<Job>  pOnConnJob;
    std::function<void( const URL&, const XRootDStatus& )> pOnConnErrCB;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  PostMaster::PostMaster(): pImpl( new PostMasterImpl() )
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  PostMaster::~PostMaster()
  {
  }

  //----------------------------------------------------------------------------
  // Initializer
  //----------------------------------------------------------------------------
  bool PostMaster::Initialize()
  {
    Env *env = DefaultEnv::GetEnv();
    std::string pollerPref = DefaultPollerPreference;
    env->GetString( "PollerPreference", pollerPref );

    pImpl->pPoller = PollerFactory::CreatePoller( pollerPref );

    if( !pImpl->pPoller )
      return false;

    bool st = pImpl->pPoller->Initialize();

    if( !st )
    {
      delete pImpl->pPoller;
      return false;
    }

    pImpl->pJobManager->Initialize();
    pImpl->pInitialized = true;
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
    if( !pImpl->pInitialized )
      return true;

    pImpl->pInitialized = false;
    pImpl->pJobManager->Finalize();

    //--------------------------------------------------------------------------
    // Finalize may cause some of the channels to remove themselves from the
    // finalize set. So make a copy. Should be no concurrency as poller and
    // jobmanager are stopped, so no lock.
    //--------------------------------------------------------------------------
    auto finSet = pImpl->pFinalizeSet;
    for( auto ch: finSet ) ch->Finalize();

    pImpl->pChannelMap.clear();
    return pImpl->pPoller->Finalize();
  }

  //----------------------------------------------------------------------------
  // Start the post master
  //----------------------------------------------------------------------------
  bool PostMaster::Start()
  {
    if( !pImpl->pInitialized )
      return false;

    if( !pImpl->pPoller->Start() )
      return false;

    if( !pImpl->pTaskManager->Start() )
    {
      pImpl->pPoller->Stop();
      return false;
    }

    if( !pImpl->pJobManager->Start() )
    {
      pImpl->pPoller->Stop();
      pImpl->pTaskManager->Stop();
      return false;
    }

    pImpl->pRunning = true;
    return true;
  }

  //----------------------------------------------------------------------------
  // Stop the postmaster
  //----------------------------------------------------------------------------
  bool PostMaster::Stop()
  {
    if( !pImpl->pInitialized || !pImpl->pRunning )
      return true;

    if( !pImpl->pJobManager->Stop() )
      return false;
    if( !pImpl->pPoller->Stop() )
      return false;
    if( !pImpl->pTaskManager->Stop() )
      return false;
    pImpl->pRunning = false;
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
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  XRootDStatus PostMaster::Send( const URL    &url,
                                 Message      *msg,
                                 MsgHandler   *handler,
                                 bool          stateful,
                                 time_t        expires )
  {
    auto channel = pImpl->GetChannel( url );

    if( !channel )
      return XRootDStatus( stError, errNotSupported );

    return channel->Send( msg, handler, stateful, expires );
  }

  Status PostMaster::Redirect( const URL  &url,
                               Message    *msg,
                               MsgHandler *inHandler )
  {
    RedirectorRegistry &registry  = RedirectorRegistry::Instance();
    VirtualRedirector *redirector = registry.Get( url );
    if( !redirector )
      return Status( stError, errInvalidOp );
    return redirector->HandleRequest( msg, inHandler );
  }

  //----------------------------------------------------------------------------
  // Query the transport handler
  //----------------------------------------------------------------------------
  Status PostMaster::QueryTransport( const URL &url,
                                     uint16_t   query,
                                     AnyObject &result )
  {
    std::shared_ptr<Channel> channel;
    {
      XrdSysMutexHelper scopedLock( pImpl->pChannelMapMutex );
      PostMasterImpl::ChannelMap::iterator it =
          pImpl->pChannelMap.find( url.GetChannelId() );
      if( it == pImpl->pChannelMap.end() )
        return Status( stError, errInvalidOp );
      channel = it->second;
    }

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
    auto channel = pImpl->GetChannel( url );

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
    auto channel = pImpl->GetChannel( url );

    if( !channel )
      return Status( stError, errNotSupported );

    channel->RemoveEventHandler( handler );
    return Status();
  }

  //------------------------------------------------------------------------
  // Get the task manager object user by the post master
  //------------------------------------------------------------------------
  TaskManager* PostMaster::GetTaskManager()
  {
    return pImpl->pTaskManager;
  }

  //------------------------------------------------------------------------
  // Get the job manager object user by the post master
  //------------------------------------------------------------------------
  JobManager* PostMaster::GetJobManager()
  {
    return pImpl->pJobManager;
  }

  //------------------------------------------------------------------------
  // Shut down a channel
  //------------------------------------------------------------------------
  Status PostMaster::ForceDisconnect( const URL &url )
  {
    return ForceDisconnect(url, false);
  }

  //------------------------------------------------------------------------
  // Shut down a channel
  //------------------------------------------------------------------------
  Status PostMaster::ForceDisconnect( const URL &url, bool hush )
  {
    std::shared_ptr<Channel> channel;
    {
      XrdSysMutexHelper scopedLock( pImpl->pChannelMapMutex );
      PostMasterImpl::ChannelMap::iterator it =
          pImpl->pChannelMap.find( url.GetChannelId() );

      if( it == pImpl->pChannelMap.end() )
        return Status( stError, errInvalidOp );
      channel = it->second;
      pImpl->pChannelMap.erase( it );
    }

    channel->ForceDisconnect( hush );
    return Status();
  }

  //------------------------------------------------------------------------
  // Shut down a channel. This version is used by the channel itself.
  //------------------------------------------------------------------------
  Status PostMaster::ForceDisconnect( std::shared_ptr<Channel> channel,
                                      const uint64_t sess )
  {
    if( !channel )
      return Status( stError, errNotSupported );

    {
      XrdSysMutexHelper scopedLock( pImpl->pChannelMapMutex );
      PostMasterImpl::ChannelMap::iterator it =
          pImpl->pChannelMap.find( channel->GetURL().GetChannelId() );

      if( it != pImpl->pChannelMap.end() && it->second == channel )
        pImpl->pChannelMap.erase( it );
    }

    channel->ForceDisconnect( channel, sess );
    return Status();
  }

  Status PostMaster::ForceReconnect( const URL &url )
  {
    std::shared_ptr<Channel> channel;
    {
      XrdSysMutexHelper scopedLock( pImpl->pChannelMapMutex );
      PostMasterImpl::ChannelMap::iterator it =
          pImpl->pChannelMap.find( url.GetChannelId() );

      if( it == pImpl->pChannelMap.end() )
        return Status( stError, errInvalidOp );
      channel = it->second;
    }

    channel->ForceReconnect();
    return Status();
  }

  //------------------------------------------------------------------------
  // Get the number of connected data streams
  //------------------------------------------------------------------------
  uint16_t PostMaster::NbConnectedStrm( const URL &url )
  {
    auto channel = pImpl->GetChannel( url );
    if( !channel ) return 0;
    return channel->NbConnectedStrm();
  }

  //------------------------------------------------------------------------
  //! Set the on-connect handler for data streams
  //------------------------------------------------------------------------
  void PostMaster::SetOnDataConnectHandler( const URL            &url,
                                            std::shared_ptr<Job>  onConnJob )
  {
    auto channel = pImpl->GetChannel( url );
    if( !channel ) return;
    channel->SetOnDataConnectHandler( onConnJob );
  }

  //------------------------------------------------------------------------
  //! Set the global on-connect handler for control streams
  //------------------------------------------------------------------------
  void PostMaster::SetOnConnectHandler( std::unique_ptr<Job> onConnJob )
  {
    XrdSysMutexHelper lck( pImpl->pMtx );
    pImpl->pOnConnJob = std::move( onConnJob );
  }

  //------------------------------------------------------------------------
  // Set the global connection error handler
  //------------------------------------------------------------------------
  void PostMaster::SetConnectionErrorHandler( std::function<void( const URL&, const XRootDStatus& )> handler )
  {
    XrdSysMutexHelper lck( pImpl->pMtx );
    pImpl->pOnConnErrCB = std::move( handler );
  }

  //------------------------------------------------------------------------
  // Notify the global on-connect handler
  //------------------------------------------------------------------------
  void PostMaster::NotifyConnectHandler( const URL &url )
  {
    XrdSysMutexHelper lck( pImpl->pMtx );
    if( pImpl->pOnConnJob )
    {
      URL *ptr = new URL( url );
      pImpl->pJobManager->QueueJob( pImpl->pOnConnJob.get(), ptr );
    }
  }

  //------------------------------------------------------------------------
  // Notify the global error connection handler
  //------------------------------------------------------------------------
  void PostMaster::NotifyConnErrHandler( const URL &url, const XRootDStatus &status )
  {
    XrdSysMutexHelper lck( pImpl->pMtx );
    if( pImpl->pOnConnErrCB )
    {
      ConnErrJob *job = new ConnErrJob( url, status, pImpl->pOnConnErrCB );
      pImpl->pJobManager->QueueJob( job, nullptr );
    }
  }

  //----------------------------------------------------------------------------
  //! Collapse channel URL - replace the URL of the channel
  //----------------------------------------------------------------------------
  void PostMaster::CollapseRedirect( const URL &alias, const URL &url )
  {
    XrdSysMutexHelper scopedLock( pImpl->pChannelMapMutex );
    //--------------------------------------------------------------------------
    // Get the passive channel
    //--------------------------------------------------------------------------
    std::shared_ptr<Channel> passive;
    PostMasterImpl::ChannelMap::iterator it =
        pImpl->pChannelMap.find( alias.GetChannelId() );
    if( it != pImpl->pChannelMap.end() )
      passive = it->second;

    //--------------------------------------------------------------------------
    // If the channel does not exist there's nothing to do
    //--------------------------------------------------------------------------
    if( !passive ) return;

    //--------------------------------------------------------------------------
    // Check if this URL is eligible for collapsing. To avoid depencencies
    // we don't call CanCollapse while holding the channel map mutex. So we
    // reverify the content of the map afterwards.
    //--------------------------------------------------------------------------
    scopedLock.UnLock();
    if( !passive->CanCollapse( url ) ) return;

    scopedLock.Lock( &pImpl->pChannelMapMutex );
    it = pImpl->pChannelMap.find( alias.GetChannelId() );
    if( it == pImpl->pChannelMap.end() || it->second != passive )
    {
      // something changed. Retry.
      scopedLock.UnLock();
      CollapseRedirect( alias, url );
      return;
    }

    //--------------------------------------------------------------------------
    // Create the active channel
    //--------------------------------------------------------------------------
    TransportManager *trManager = DefaultEnv::GetTransportManager();
    TransportHandler *trHandler = trManager->GetHandler( url.GetProtocol() );

    if( !trHandler )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( PostMasterMsg, "Unable to get transport handler for %s "
                  "protocol", url.GetProtocol().c_str() );
      return;
    }

    Log *log = DefaultEnv::GetLog();
    log->Info( PostMasterMsg, "Label channel %s with alias %s.",
               url.GetHostId().c_str(), alias.GetHostId().c_str() );

    std::shared_ptr<Channel> active(new Channel{ alias,
      pImpl->pPoller, trHandler, pImpl->pTaskManager, pImpl->pJobManager, url },
      [this](Channel *ch) { this->pImpl->removeFinalize( ch ); delete ch; });
    pImpl->addFinalize( active.get() );
    active->SetSelf( active );

    pImpl->pChannelMap[alias.GetChannelId()] = active;
    //--------------------------------------------------------------------------
    // The passive channel will be deallocated by TTL
    //--------------------------------------------------------------------------
  }

  //------------------------------------------------------------------------
  // Decrement file object instance count bound to this channel
  //------------------------------------------------------------------------
  void PostMaster::DecFileInstCnt( const URL &url )
  {
    auto channel = pImpl->GetChannel( url );

    if( !channel ) return;

    return channel->DecFileInstCnt();
  }

  //------------------------------------------------------------------------
  //true if underlying threads are running, false otherwise
  //------------------------------------------------------------------------
  bool PostMaster::IsRunning()
  {
    return pImpl->pRunning;
  }

  //----------------------------------------------------------------------------
  // Get the channel
  //----------------------------------------------------------------------------
  std::shared_ptr<Channel> PostMasterImpl::GetChannel( const URL &url )
  {
    XrdSysMutexHelper scopedLock( pChannelMapMutex );
    std::shared_ptr<Channel> channel;
    PostMasterImpl::ChannelMap::iterator it = pChannelMap.find( url.GetChannelId() );

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

      std::shared_ptr<Channel> newchan(new Channel{ url, pPoller,
        trHandler, pTaskManager, pJobManager },
        [this](Channel *ch) { this->removeFinalize( ch ); delete ch; });
      addFinalize( newchan.get() );
      channel = newchan;
      channel->SetSelf( channel );

      pChannelMap[url.GetChannelId()] = channel;
    }
    else
      channel = it->second;
    return channel;
  }
}
