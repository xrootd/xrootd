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
    PostMasterImpl() : pPoller( 0 ), pInitialized( false )
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

    typedef std::map<std::string, Channel*> ChannelMap;

    Poller               *pPoller;
    TaskManager          *pTaskManager;
    ChannelMap            pChannelMap;
    XrdSysMutex           pChannelMapMutex;
    bool                  pInitialized;
    JobManager           *pJobManager;

    XrdSysMutex           pMtx;
    std::unique_ptr<Job>  pOnConnJob;
    std::function<void( const URL&, const XRootDStatus& )> pOnConnErrCB;

    XrdSysRWLock          pDisconnectLock;
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
    PostMasterImpl::ChannelMap::iterator it;

    for( it = pImpl->pChannelMap.begin(); it != pImpl->pChannelMap.end(); ++it )
      delete it->second;

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

    return true;
  }

  //----------------------------------------------------------------------------
  // Stop the postmaster
  //----------------------------------------------------------------------------
  bool PostMaster::Stop()
  {
    if( !pImpl->pInitialized )
      return true;

    if( !pImpl->pJobManager->Stop() )
      return false;
    if( !pImpl->pTaskManager->Stop() )
      return false;
    if( !pImpl->pPoller->Stop() )
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
  XRootDStatus PostMaster::Send( const URL &url,
                                 Message   *msg,
                                 bool       stateful,
                                 time_t     expires )
  {
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
    Channel *channel = GetChannel( url );

    if( !channel )
      return XRootDStatus( stError, errNotSupported );

    return channel->Send( msg, stateful, expires );
  }

  //----------------------------------------------------------------------------
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  XRootDStatus PostMaster::Send( const URL            &url,
                                 Message              *msg,
                                 OutgoingMsgHandler   *handler,
                                 bool                  stateful,
                                 time_t                expires )
  {
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
    Channel *channel = GetChannel( url );

    if( !channel )
      return XRootDStatus( stError, errNotSupported );

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
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
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
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
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
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
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
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
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
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
    Channel *channel = GetChannel( url );

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
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock, false );
    PostMasterImpl::ChannelMap::iterator it =
        pImpl->pChannelMap.find( url.GetChannelId() );

    if( it == pImpl->pChannelMap.end() )
      return Status( stError, errInvalidOp );

    it->second->ForceDisconnect();
    delete it->second;
    pImpl->pChannelMap.erase( it );

    return Status();
  }

  //------------------------------------------------------------------------
  // Get the number of connected data streams
  //------------------------------------------------------------------------
  uint16_t PostMaster::NbConnectedStrm( const URL &url )
  {
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
    Channel *channel = GetChannel( url );
    if( !channel ) return 0;
    return channel->NbConnectedStrm();
  }

  //------------------------------------------------------------------------
  //! Set the on-connect handler for data streams
  //------------------------------------------------------------------------
  void PostMaster::SetOnDataConnectHandler( const URL            &url,
                                            std::shared_ptr<Job>  onConnJob )
  {
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
    Channel *channel = GetChannel( url );
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
    PostMasterImpl::ChannelMap::iterator it =
        pImpl->pChannelMap.find( alias.GetChannelId() );
    Channel *passive = 0;
    if( it != pImpl->pChannelMap.end() )
      passive = it->second;
    //--------------------------------------------------------------------------
    // If the channel does not exist there's nothing to do
    //--------------------------------------------------------------------------
    else return;

    //--------------------------------------------------------------------------
    // Check if this URL is eligible for collapsing
    //--------------------------------------------------------------------------
    if( !passive->CanCollapse( url ) ) return;

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
    log->Info( PostMasterMsg, "Lable channel %s with alias %s.",
               url.GetHostId().c_str(), alias.GetHostId().c_str() );

    Channel *active = new Channel( alias, pImpl->pPoller, trHandler,
                                   pImpl->pTaskManager, pImpl->pJobManager, url );
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
    XrdSysRWLockHelper scopedLock( pImpl->pDisconnectLock );
    Channel *channel = GetChannel( url );

    if( !channel ) return;

    return channel->DecFileInstCnt();
  }

  //----------------------------------------------------------------------------
  // Get the channel
  //----------------------------------------------------------------------------
  Channel *PostMaster::GetChannel( const URL &url )
  {
    XrdSysMutexHelper scopedLock( pImpl->pChannelMapMutex );
    Channel *channel = 0;
    PostMasterImpl::ChannelMap::iterator it = pImpl->pChannelMap.find( url.GetChannelId() );

    if( it == pImpl->pChannelMap.end() )
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

      channel = new Channel( url, pImpl->pPoller, trHandler, pImpl->pTaskManager,
                             pImpl->pJobManager );
      pImpl->pChannelMap[url.GetChannelId()] = channel;
    }
    else
      channel = it->second;
    return channel;
  }
}
