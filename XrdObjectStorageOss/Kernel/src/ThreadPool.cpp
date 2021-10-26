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

#include <Visus/ThreadPool.h>

namespace Visus {

////////////////////////////////////////////////////////////
ThreadPool::ThreadPool(String basename,int num_workers) 
{
  for (int I=0;I<num_workers;I++)
  {
    String thread_name = basename + " " + cstring(I);

    this->threads.push_back(Thread::start(thread_name, [this,I]() {
      workerEntryProc(I);
    }));
  }
}

////////////////////////////////////////////////////////////
ThreadPool::~ThreadPool()
{
  //each worker should get only one!
  for (auto thread : threads)
    asyncRun(std::function<void()>());

  for (auto thread : threads) 
    Thread::join(thread);

  #ifdef _DEBUG
  {
    ScopedLock lock(this->lock);
    int total_jobs=(int)waiting.size()+(int)running.size();
    VisusAssert(total_jobs==0);
  }
  #endif
}


////////////////////////////////////////////////////////////
void ThreadPool::asyncRun(std::function<void()> run)
{
  {
    ScopedLock lock(wait_all.lock);
    wait_all.num_inside++;
  }

  ThreadPool::global_stats()->running_jobs++;

  {
    ScopedLock lock(this->lock);
    waiting.push_back(std::make_shared<std::function<void()>>(run));
  }

  nwaiting.up();
}

////////////////////////////////////////////////////////////
void ThreadPool::push(SharedPtr<ThreadPool> pool, std::function<void()> fn) {
  if (pool)
    pool->asyncRun(fn);
  else
    fn();
}


////////////////////////////////////////////////////////////
void ThreadPool::waitAll() 
{
  while (true)
  {
    {
      ScopedLock lock(wait_all.lock);
      if (!wait_all.num_inside)
        return;
    }

    //note: possible deadlocks if I have the python GIL here
    wait_all.num_done.down();

    {
      ScopedLock lock(wait_all.lock);
      --wait_all.num_inside;
    }

  }
};


////////////////////////////////////////////////////////////
void ThreadPool::workerEntryProc(int worker)
{
  while (true)
  {
    this->nwaiting.down();

    //waiting->running
    SharedPtr<std::function<void()> > fn;
    {
      ScopedLock lock(this->lock);
      fn = this->waiting.front();
      this->waiting.pop_front();
      this->running.insert(fn);
    }

    bool bExit = *fn ? false : true;

    if (!bExit)
      (*fn)();

    //remove from running
    {
      ScopedLock lock(this->lock);
      VisusAssert(running.find(fn) != running.end());
      this->running.erase(fn);
    }

    ThreadPool::global_stats()->running_jobs--;

    //for the wait all function
    {
      ScopedLock lock(wait_all.lock);
      wait_all.num_done.up();
    }

    if (bExit)
      break;
  }
}


} //namespace Visus


