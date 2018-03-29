//------------------------------------------------------------------------------
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
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

#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

//------------------------------------------------------------------------------
// The thread
//------------------------------------------------------------------------------
extern "C"
{
  static void *RunRunnerThread( void *arg )
  {
    using namespace XrdCl;
    JobManager *mgr = (JobManager*)arg;
    mgr->RunJobs();
    return 0;
  }
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Initialize the job manager
  //----------------------------------------------------------------------------
  bool JobManager::Initialize()
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Finalize the job manager, clear the queues
  //----------------------------------------------------------------------------
  bool JobManager::Finalize()
  {
    pJobs.Clear();
    return true;
  }

  //----------------------------------------------------------------------------
  // Start the workers
  //----------------------------------------------------------------------------
  bool JobManager::Start()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = DefaultEnv::GetLog();
    log->Debug( JobMgrMsg, "Starting the job manager..." );

    if( pRunning )
    {
      log->Error( JobMgrMsg, "The job manager is already running" );
      return false;
    }

    for( uint32_t i = 0; i < pWorkers.size(); ++i )
    {
      int ret = ::pthread_create( &pWorkers[i], 0, ::RunRunnerThread, this );
      if( ret != 0 )
      {
        log->Error( JobMgrMsg, "Unable to spawn a job worker thread: %s",
                    strerror( errno ) );
        if( i > 0 )
          StopWorkers( i-1 );
        return false;
      }
    }
    pRunning = true;
    log->Debug( JobMgrMsg, "Job manager started, %d workers", pWorkers.size() );
    return true;
  }

  //----------------------------------------------------------------------------
  // Stop the workers
  //----------------------------------------------------------------------------
  bool JobManager::Stop()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = DefaultEnv::GetLog();
    log->Debug( JobMgrMsg, "Stopping the job manager..." );
    if( !pRunning )
    {
      log->Error( JobMgrMsg, "The job manager is not running" );
      return false;
    }

    StopWorkers( pWorkers.size()-1 );

    pRunning = false;
    log->Debug( JobMgrMsg, "Job manager stopped" );
    return true;
  }

  //----------------------------------------------------------------------------
  // Stop all workers up to n'th
  //----------------------------------------------------------------------------
  void JobManager::StopWorkers( uint32_t n )
  {
    Log *log = DefaultEnv::GetLog();
    for( uint32_t i = 0; i <= n; ++i )
    {
      void *threadRet;
      log->Dump( JobMgrMsg, "Stopping worker #%d...", i );
      if( pthread_cancel( pWorkers[i] ) != 0 )
      {
        log->Error( TaskMgrMsg, "Unable to cancel worker #%d: %s", i,
                    strerror( errno ) );
        abort();
      }
      
      if( pthread_join( pWorkers[i], (void**)&threadRet ) != 0 )
      {
        log->Error( TaskMgrMsg, "Unable to join worker #%d: %s", i,
                    strerror( errno ) );
        abort();
      }

      log->Dump( JobMgrMsg, "Worker #%d stopped", i );
    }
  }

  //----------------------------------------------------------------------------
  // Initialize the job manager
  //----------------------------------------------------------------------------
  void JobManager::RunJobs()
  {
    pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, 0 );
    for( ;; )
    {
      JobHelper h = pJobs.Get();
      pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, 0 );
      h.job->Run( h.arg );
      pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, 0 );
    }
  }
}
