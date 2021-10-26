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

#ifndef __VISUS_CRITICAL_SECTION_H__
#define __VISUS_CRITICAL_SECTION_H__

#include <Visus/Kernel.h>

#include <mutex>

namespace Visus {


typedef std::mutex CriticalSection;
typedef std::lock_guard<std::mutex> ScopedLock;

///////////////////////////////////////////////////
#if !SWIG
class VISUS_KERNEL_API RWLock
{
public:

  VISUS_PIMPL_CLASS(RWLock)

  //constructor
  RWLock();

  //destructor
  ~RWLock();

  //enter 
  void enterRead();

  //exitRead
  void exitRead();

  //enterWrite
  void enterWrite();

  //exitWrite
  void exitWrite();

};


//////////////////////////////////////////////////////////////
class VISUS_KERNEL_API ScopedReadLock
{
public:

  VISUS_NON_COPYABLE_CLASS(ScopedReadLock)

  RWLock* lock=nullptr;

  //constructor from a pointer (can be ZERO, in this case no critical section is used)
  explicit ScopedReadLock(RWLock* lock_=nullptr) : lock(lock_) {
    if (lock)
      lock->enterRead();
  }

  //constructor (call with "r" for reading, call with "w" for writing)
  explicit ScopedReadLock(RWLock& lock_) : ScopedReadLock(&lock_) {
  }

  //destructor
  inline ~ScopedReadLock() {
    if (lock) 
      lock->exitRead();
  }

};

//////////////////////////////////////////////////////////////
class VISUS_KERNEL_API ScopedWriteLock : public ScopedReadLock
{
public:

  VISUS_NON_COPYABLE_CLASS(ScopedWriteLock)

  //constructor from a pointer (can be ZERO, in this case no critical section is used)
  explicit ScopedWriteLock(RWLock* lock) : ScopedReadLock(nullptr) {
    this->lock = lock;
    if (lock)
      lock->enterWrite();
  }

  //constructor (call with "r" for reading, call with "w" for writing)
  explicit ScopedWriteLock(RWLock& lock) : ScopedWriteLock(&lock) {
  }

  //constructor
  explicit ScopedWriteLock(ScopedReadLock& rlock) : ScopedWriteLock(nullptr) {
    this->lock = rlock.lock;
    if (this->lock)
    {
      bWasReading = true;
      this->lock->exitRead();
      this->lock->enterWrite();
    }
  }

  //destructor
  ~ScopedWriteLock() 
  {
    if (this->lock)
    {
      lock->exitWrite();
      if (bWasReading)
        lock->enterRead();
    }

    //avoid the ScopedReadLock to do anything
    lock = nullptr;
  }

private:

  bool bWasReading = false;

};
#endif //#if !SWIG

} //namespace Visus

#endif  //__VISUS_CRITICAL_SECTION_H__
