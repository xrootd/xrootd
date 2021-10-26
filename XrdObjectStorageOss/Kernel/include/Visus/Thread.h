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

#ifndef __VISUS_THREAD_H__
#define __VISUS_THREAD_H__

#include <Visus/Kernel.h>
#include <Visus/Aborted.h>
#include <Visus/Semaphore.h>
#include <Visus/CriticalSection.h>

#include <thread>
#include <atomic>
#include <set>
#include <deque>
#include <map>
#include <functional>

namespace Visus {

  //////////////////////////////////////////////////////////////
class VISUS_KERNEL_API ThreadGlobalStats
{
public:

  VISUS_NON_COPYABLE_CLASS(ThreadGlobalStats)

#if !SWIG
  std::atomic<Int64> running_threads;
#endif

  //constructor
  ThreadGlobalStats() : running_threads(0){
  }

  //getNumRunningThreads
  Int64 getNumRunningThreads() {
    return running_threads;
  }


};


  //////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Thread 
{
public:

  //global_stats
  static ThreadGlobalStats* global_stats() {
    static ThreadGlobalStats ret;
    return &ret;
  }

  // start
  static SharedPtr<std::thread> start(String name,std::function<void()> entry_proc);

  //join
  static void join(SharedPtr<std::thread> thread) 
  {
    if (thread && thread->joinable())
      thread->join();
  }

  //join
  static void join(SharedPtr<std::thread> thread,bool& bExit) 
  {
    if (thread && thread->joinable())
    {
      bExit=true;
      thread->join();
    }
  }

  //sleep
  static void sleep(int msec) {
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
  }

  //yield
#if !SWIG
  static void yield() {
    std::this_thread::yield();
  }
#endif

  //getThreadId
  static std::thread::id getThreadId() {
    return std::this_thread::get_id();
  }

  //getMainThreadId
  static std::thread::id& getMainThreadId() {
    static std::thread::id ret; return ret;
  }

  //if the current thread is the main thread
  static bool isMainThread() {
    return getThreadId()==getMainThreadId();
  }

private:

  //only static
  Thread()=delete;

}; //end class

} //namespace Visus

#endif  //__VISUS_THREAD_H__



