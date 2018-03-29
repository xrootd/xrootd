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

#ifndef __XRD_CL_TASK_MANAGER_HH__
#define __XRD_CL_TASK_MANAGER_HH__

#include <ctime>
#include <set>
#include <list>
#include <string>
#include <stdint.h>
#include <pthread.h>
#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Interface for a task to be run by the TaskManager
  //----------------------------------------------------------------------------
  class Task
  {
    public:
      virtual ~Task() {};

      //------------------------------------------------------------------------
      //! Perform the task
      //!
      //! @param now current timestamp
      //! @return 0 if the task is completed and should no longer be run or
      //!         the time at which it should be run again
      //------------------------------------------------------------------------
      virtual time_t Run( time_t now ) = 0;

      //------------------------------------------------------------------------
      //! Name of the task
      //------------------------------------------------------------------------
      const std::string &GetName() const
      {
        return pName;
      }

      //------------------------------------------------------------------------
      //! Set name of the task
      //------------------------------------------------------------------------
      void SetName( const std::string &name )
      {
        pName = name;
      }

    private:
      std::string pName;
  };

  //----------------------------------------------------------------------------
  //! Run short tasks at a given time in the future
  //!
  //! The task manager just runs one extra thread so the execution of one tasks
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
      //! Destructor
      //------------------------------------------------------------------------
      ~TaskManager();

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
      //! Run the given task at the given time.
      //!
      //! @param task task to be run
      //! @param time time at which the task should be run
      //! @param own  determines whether the task object should be destroyed
      //!             when no longer needed
      //------------------------------------------------------------------------
      void RegisterTask( Task *task, time_t time, bool own = true );

      //------------------------------------------------------------------------
      //! Remove a task, the unregistration process is asynchronous and may
      //! be performed at any point in the future, the function just queues
      //! the request. Unregistered task gets destroyed if it was owned by
      //! the task manager.
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
        TaskHelper( Task *tsk, time_t tme, bool ow = true ):
          task(tsk), execTime(tme), own(ow) {}
        Task   *task;
        time_t  execTime;
        bool    own;
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
      XrdSysMutex pOpMutex;
  };
}

#endif // __XRD_CL_TASK_MANAGER_HH__
