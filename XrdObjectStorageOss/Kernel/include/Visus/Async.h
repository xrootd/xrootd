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

#ifndef _VISUS_ASYNC_H__
#define _VISUS_ASYNC_H__

#include <Visus/Kernel.h>
#include <Visus/Semaphore.h>
#include <Visus/CriticalSection.h>

#include <list>

namespace Visus {


//using custom class because C++11 cannot wait for multiple future 
//see http://stackoverflow.com/questions/19225372/waiting-for-multiple-futures

///////////////////////////////////////////////////////////
template <typename __Value__>
class BasePromise
{
public:

  typedef __Value__ Value;

  CriticalSection  lock;
  SharedPtr<Value> value;

  typedef std::function<void(Value)> Callback;

  //constructor
  BasePromise() {
  }

  //set_value
  void set_value(const Value& value)
  {
    std::vector<Callback> callbacks;
    {
      ScopedLock lock(this->lock);
      VisusAssert(!this->value);
      this->value = std::make_shared<Value>(value);
      callbacks =this->callbacks;
      this->callbacks.clear(); //one shot
    }

    for (auto fn : callbacks)
      fn(value);
  }

  //addWhenDoneListener (must have the lock and value must not exists)
  void addWhenDoneListener(Callback fn) {
    VisusAssert(!value);
    callbacks.push_back(fn);
  }

  //isDone
  bool is_ready() const {
    ScopedLock lock(const_cast<BasePromise*>(this)->lock);
    return value? true : false;
  }

  //when_ready
  void when_ready(Callback fn) {
    this->lock.lock();
    if (value) {
      this->lock.unlock();
      fn(*value);
    }
    else
    {
      addWhenDoneListener(fn);
      this->lock.unlock();
    }
  }

private:

  VISUS_NON_COPYABLE_CLASS(BasePromise)

  std::vector< Callback > callbacks;

};

///////////////////////////////////////////////////////////
template <typename __Value__>
class Future 
{
public:

  typedef __Value__ Value;

  //default constructor
  Future() {
  }

  //constructor
  Future(SharedPtr< BasePromise<Value> > promise_) : promise(promise_) {
  }

  //copy constructor
  Future(const Future& other) : Future(other.promise) {
  }

  //constructor
  ~Future() {
  }

  //operator=
  Future& operator=(const Future& other) {
    this->promise=other.promise;
    return *this;
  }

  //get
  Value get() const
  {
    ScopedLock lock(promise->lock);
    
    //need to wait?
    if (!promise->value)
    {
      promise->addWhenDoneListener([this](Value){
        const_cast<Future*>(this)->ready.up();
      });
      promise->lock.unlock();
      const_cast<Future*>(this)->ready.down();
      promise->lock.lock();
      VisusAssert(promise->value);
    }

    return *(promise->value);
  }

  //get_promise
  SharedPtr< BasePromise<Value> > get_promise() const {
    return promise;
  }

  //isDone
  bool is_ready() const {
    if (!promise) { VisusAssert(false); return false;}
    return promise->is_ready();
  }

  //when_ready
  void when_ready(typename BasePromise<Value>::Callback fn) {
     return promise->when_ready(fn);
  }

private:

  SharedPtr< BasePromise<Value> > promise;
  Semaphore                       ready;

};

///////////////////////////////////////////////////////////
template <typename __Value__>
class Promise
{
public:

  typedef __Value__ Value;

  //constructor
  Promise() {
  }

  //constructor
  Promise(const Value& value) {
    set_value(value);
  }

  //set_value
  void set_value(const Value& value) {
    base_promise->set_value(value);
  }

  //get_future
  Future<Value> get_future() {
    return Future<Value>(base_promise);
  }

  //when_ready
  void when_ready(typename BasePromise<Value>::Callback fn) {
    base_promise->when_ready(fn);
  }

private:

  SharedPtr< BasePromise<Value> > base_promise =std::make_shared< BasePromise<Value> >() ;

}; 


///////////////////////////////////////////////////////////
template <typename Future>
class WaitAsync 
{
public:

  typedef typename Future::Value Value;
  typedef typename BasePromise<Value>::Callback Callback;

  //constructor
  WaitAsync(int max_running_=0) : max_running(max_running_)  {
  }

  //destructor
  ~WaitAsync() {
  }

  //pushRunning
  void pushRunning(Future future, Callback fn)
  {
    //there is a limit about how much to push
    while (this->max_running > 0 && this->num_running >= this->max_running)
      waitOneDone();

    
    auto promise = future.get_promise();
    VisusAssert(promise);

    promise->lock.lock();

    // immediate call of the callback
    if (auto value = promise->value)
    {
      promise->lock.unlock();
      fn(*value);
    }
    else
    {
      //need to wait, as soon as it becomes available I'm moving it to done deque
      ++num_running;
      promise->addWhenDoneListener(Callback([this, fn](Value value) {
        ScopedLock this_lock(this->lock);
        this->done.push_front(std::make_pair(fn, value));
        this->semaphore.up();
      }));

      promise->lock.unlock();
    }
  }

  //waitOneDone
  void waitOneDone() 
  {
    Callback fn; Value value;
    this->semaphore.down();
    {
      ScopedLock lock(this->lock);
      VisusReleaseAssert(!this->done.empty());
      fn    = this->done.back().first;
      value = this->done.back().second;
      this->done.pop_back();
    }

    --this->num_running;
    fn(value); 
  }

  //waitAllDone
  void waitAllDone()
  {
    while (num_running)
      waitOneDone();
  }

private:

  VISUS_NON_COPYABLE_CLASS(WaitAsync)

  CriticalSection                         lock;
  Semaphore                               semaphore;
  std::deque< std::pair<Callback,Value> > done;

  int                                     max_running = 0;
  int                                     num_running = 0;

};



} //namespace Visus

#endif //_VISUS_ASYNC_H__

