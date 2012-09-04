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

#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

#include <iostream>

//------------------------------------------------------------------------------
// The thread
//------------------------------------------------------------------------------
extern "C"
{
  static void *RunRunnerThread( void *arg )
  {
    using namespace XrdCl;
    TaskManager *mgr = (TaskManager*)arg;
    mgr->RunTasks();
    return 0;
  }
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  TaskManager::TaskManager(): pResolution(1), pRunnerThread(0), pRunning(false)
  {}

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  TaskManager::~TaskManager()
  {
    TaskSet::iterator  it, itE;
    for( it = pTasks.begin(); it != pTasks.end(); ++it )
      delete it->task;
  }

  //----------------------------------------------------------------------------
  // Start the manager
  //----------------------------------------------------------------------------
  bool TaskManager::Start()
  {
    XrdSysMutexHelper scopedLock( pOpMutex );
    Log *log = DefaultEnv::GetLog();
    log->Debug( TaskMgrMsg, "Starting the task manager..." );

    if( pRunning )
    {
      log->Error( TaskMgrMsg, "The task manager is already running" );
      return false;
    }

    int ret = ::pthread_create( &pRunnerThread, 0, ::RunRunnerThread, this );
    if( ret != 0 )
    {
      log->Error( TaskMgrMsg, "Unable to spawn the task runner thread: %s",
                              strerror( errno ) );
      return false;
    }
    pRunning = true;
    log->Debug( TaskMgrMsg, "Task manager started" );
    return true;
  }

  //----------------------------------------------------------------------------
  // Stop the manager
  //----------------------------------------------------------------------------
  bool TaskManager::Stop()
  {
    XrdSysMutexHelper scopedLock( pOpMutex );
    Log *log = DefaultEnv::GetLog();
    log->Debug( TaskMgrMsg, "Stopping the task manager..." );
    if( !pRunning )
    {
      log->Error( TaskMgrMsg, "The task manager is not running" );
      return false;
    }

    if( ::pthread_cancel( pRunnerThread ) != 0 )
    {
      log->Error( TaskMgrMsg, "Unable to cancel the task runner thread: %s",
                              strerror( errno ) );
      return false;
    }

    void *threadRet;
    int ret = pthread_join( pRunnerThread, (void **)&threadRet );
    if( ret != 0 )
    {
      log->Error( TaskMgrMsg, "Failed to join the task runner thread: %s",
                               strerror( errno ) );
      return false;
    }

    pRunning = false;
    log->Debug( TaskMgrMsg, "Task manager stopped" );
    return true;
  }

  //----------------------------------------------------------------------------
  // Run the given task at the given time
  //----------------------------------------------------------------------------
  void TaskManager::RegisterTask( Task *task, time_t time )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( TaskMgrMsg, "Registering task: \"%s\" to be run at: %d",
                            task->GetName().c_str(), time );

    XrdSysMutexHelper scopedLock( pMutex );
    pTasks.insert( TaskHelper( task, time ) );
  }

  //--------------------------------------------------------------------------
  // Remove a task if it hasn't run yet
  //--------------------------------------------------------------------------
  void TaskManager::UnregisterTask( Task *task )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( TaskMgrMsg, "Requesting unregistration of: \"%s\"",
                            task->GetName().c_str() );
    XrdSysMutexHelper scopedLock( pMutex );
    pToBeUnregistered.push_back( task );
  }

  //----------------------------------------------------------------------------
  // Run tasks
  //----------------------------------------------------------------------------
  void TaskManager::RunTasks()
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We want the thread to be cancelable only when we sleep between tasks
    // execution
    //--------------------------------------------------------------------------
    pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, 0 );

    for(;;)
    {
      pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, 0 );
      pMutex.Lock();

      //------------------------------------------------------------------------
      // Remove the tasks from the active set - super inefficient,
      // but, hopefuly, never really necessary. We first need tu build a list
      // of iterators because it is impossible to remove elements from
      // a multiset when iterating over it
      //------------------------------------------------------------------------
      TaskList::iterator listIt = pToBeUnregistered.begin();
      TaskSet::iterator  it, itE;
      std::list<TaskSet::iterator> iteratorList;
      std::list<TaskSet::iterator>::iterator itRem;
      for( ; listIt != pToBeUnregistered.end(); ++listIt )
      {
        for( it = pTasks.begin(); it != pTasks.end(); ++it )
        {
          if( it->task == *listIt )
            iteratorList.push_back( it );
        }
      }

      for( itRem = iteratorList.begin(); itRem != iteratorList.end(); ++itRem )
      {
        Task *tsk = (*itRem)->task;
        log->Debug( TaskMgrMsg, "Removing task: \"%s\"", tsk->GetName().c_str() );
        pTasks.erase( *itRem );
        delete tsk;
      }

      pToBeUnregistered.clear();

      //------------------------------------------------------------------------
      // Select the tasks to be run
      //------------------------------------------------------------------------
      time_t   now = time(0);
      TaskList toRun;

      it  = pTasks.begin();
      itE = pTasks.upper_bound( TaskHelper( 0, now ) );

      for( ; it != itE; ++it )
        toRun.push_back( it->task );

      pTasks.erase( pTasks.begin(), itE );
      pMutex.UnLock();

      //------------------------------------------------------------------------
      // Run the tasks and reinsert them if necessary
      //------------------------------------------------------------------------
      for( listIt = toRun.begin(); listIt != toRun.end(); ++listIt )
      {
        log->Dump( TaskMgrMsg, "Running task: \"%s\"",
                   (*listIt)->GetName().c_str() );
        time_t schedule = (*listIt)->Run( now );
        if( schedule )
        {
          log->Dump( TaskMgrMsg, "Will rerun task \"%s\" at %d",
                     (*listIt)->GetName().c_str(), schedule );
          pMutex.Lock();
          pTasks.insert( TaskHelper( *listIt, schedule ) );
          pMutex.UnLock();
        }
        else
        {
          log->Debug( TaskMgrMsg, "Done with task: \"%s\"",
                      (*listIt)->GetName().c_str() );
          delete *listIt;
        }
      }

      //------------------------------------------------------------------------
      // Enable the cancelation and go to sleep
      //------------------------------------------------------------------------
      pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, 0 );
      ::sleep( pResolution );
    }
  }
}
