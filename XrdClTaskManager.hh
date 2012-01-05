//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_TASK_MANAGER_HH__
#define __XRD_CL_TASK_MANAGER_HH__

#include <ctime>
#include <set>
#include <list>
#include <stdint.h>
#include <pthread.h>
#include "XrdSys/XrdSysPthread.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Interface for a task to be run by the TaskManager
  //----------------------------------------------------------------------------
  class Task
  {
    public:
      //------------------------------------------------------------------------
      //! Destructor
      //!
      //! The manager takes the ownershit over a task so we need to be able
      //! to properly destroy it
      //------------------------------------------------------------------------
      virtual ~Task() {};

      //------------------------------------------------------------------------
      //! Perform the task
      //!
      //! @param now current timestamp
      //! @return 0 if the task is completed and should no longer be run or
      //!         the time at which it should be run again
      //------------------------------------------------------------------------
      virtual time_t Run( time_t now ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Run short tasks at a given time in the future
  //!
  //! The task manager just runs one extra thread so the execution of one taks
  //! may interfere with the execution of another
  //----------------------------------------------------------------------------
  class TaskManager
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      TaskManager();

      //------------------------------------------------------------------------
      //! Start the manager
      //------------------------------------------------------------------------
      bool Start();

      //------------------------------------------------------------------------
      //! Stop the manager
      //!
      //! Will wait until the currently running task completes
      //------------------------------------------------------------------------
      bool Stop();

      //------------------------------------------------------------------------
      //! Run the given task at the given time, the manager takes ownership
      //! over the task and will destroy it when no longer necessary
      //!
      //! @param task task to be run
      //! @param time time at which the task schould be run
      //------------------------------------------------------------------------
      void RegisterTask( Task *task, time_t time );

      //------------------------------------------------------------------------
      //! Remove a task, the unregistration process is asynchronous and may
      //! be performed at any point in the future, the function just queues
      //! the request. Unregistered task gets destroyed.
      //------------------------------------------------------------------------
      void UnregisterTask( Task *task );

      //------------------------------------------------------------------------
      //! Run the tasks - this loops infinitely
      //------------------------------------------------------------------------
      void RunTasks();

    private:

      //------------------------------------------------------------------------
      // Task set helpers
      //------------------------------------------------------------------------
      struct TaskHelper
      {
        TaskHelper( Task *tsk, time_t tme ): task(tsk), execTime(tme) {}
        Task   *task;
        time_t  execTime;
      };

      struct TaskHelperCmp
      {
        bool operator () ( const TaskHelper &th1, const TaskHelper &th2 ) const
        {
          return th1.execTime < th2.execTime;
        }
      };

      typedef std::multiset<TaskHelper, TaskHelperCmp> TaskSet;
      typedef std::list<Task*>                         TaskList;

      //------------------------------------------------------------------------
      // Private variables
      //------------------------------------------------------------------------
      uint16_t    pResolution;
      TaskSet     pTasks;
      TaskList    pToBeUnregistered;
      pthread_t   pRunnerThread;
      bool        pRunning;
      XrdSysMutex pMutex;
  };
}

#endif // __XRD_CL_TASK_MANAGER_HH__
