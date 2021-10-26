/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef __VISUS_THREAD_POOL_H__
#define __VISUS_THREAD_POOL_H__

#include <Visus/Kernel.h>
#include <Visus/Thread.h>

#include <vector>
#include <set>
#include <deque>
#include <atomic>

namespace Visus {

////////////////////////////////////////////////////////
class VISUS_KERNEL_API ThreadPoolGlobalStats
{
public:

  VISUS_NON_COPYABLE_CLASS(ThreadPoolGlobalStats)

#if !SWIG
  std::atomic<Int64> running_jobs;
#endif

  //constructor
  ThreadPoolGlobalStats() : running_jobs(0) {
  }

  //getNumRunningJobs
  Int64 getNumRunningJobs() {
    return running_jobs;
  }

};

////////////////////////////////////////////////////////
class VISUS_KERNEL_API ThreadPool
{
public:

  VISUS_NON_COPYABLE_CLASS(ThreadPool)

  //global_stats
  static ThreadPoolGlobalStats* global_stats() {
    static ThreadPoolGlobalStats ret;
    return &ret;
  }

  //constructor
  ThreadPool(String basename,int num_workers);

  //destructor
  virtual ~ThreadPool();

  //waitAll
  void waitAll();

  //push
  static void push(SharedPtr<ThreadPool> pool, std::function<void()> fn);

private:

  //___________________________________________
  class WaitAll
  {
  public:
    CriticalSection  lock;
    int              num_inside = 0;
    Semaphore        num_done;
    WaitAll() : num_inside(0) {}
  };


  CriticalSection                                   lock;
  std::vector< SharedPtr<std::thread> >             threads;
  Semaphore                                         nwaiting;
  std::deque< SharedPtr<std::function<void()> > >   waiting;
  std::set  < SharedPtr<std::function<void()> > >   running;
  WaitAll                                           wait_all;

  //workerEntryProc
  void workerEntryProc(int worker);

  //asyncRun
  void asyncRun(std::function<void()> fn);

};

} //namespace Visus


#endif  //__VISUS_THREAD_POOL_H__
