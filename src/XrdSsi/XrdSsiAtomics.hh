#ifndef __SSIATOMICS_HH__
#define __SSIATOMICS_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i A t o m i c s . h h                       */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <cstring>

#undef NEED_ATOMIC_MUTEX

//-----------------------------------------------------------------------------
//! Use native atomics at the c11 or higher level (-std=c++0x -lstdc++)
//-----------------------------------------------------------------------------
#if __cplusplus >= 201103L
#include <atomic>
#define Atomic(type)    std::atomic<type>
#define Atomic_IMP "C++11"
#define Atomic_BEG(x)
#define Atomic_DEC(x)          x.fetch_sub(1,std::memory_order_relaxed)
#define Atomic_GET(x)          x.load(std::memory_order_relaxed)
#define Atomic_GET_STRICT(x)   x.load(std::memory_order_acquire)
#define Atomic_INC(x)          x.fetch_add(1,std::memory_order_relaxed)
#define Atomic_SET(x,y)        x.store(y,std::memory_order_relaxed)
#define Atomic_SET_STRICT(x,y) x.store(y,std::memory_order_release)
#define Atomic_ZAP(x)          x.store(0,std::memory_order_relaxed)
#define Atomic_END(x)

//-----------------------------------------------------------------------------
//! Use new gcc builtins at 4.7 or above
//-----------------------------------------------------------------------------
#elif __GNUC__ == 4 && __GNUC_MINOR__ > 6
#define Atomic(type)    type
#define Atomic_IMP "gnu-atomic"
#define Atomic_BEG(x)
#define Atomic_DEC(x)          __atomic_fetch_sub(&x,1,__ATOMIC_RELAXED)
#define Atomic_GET(x)          __atomic_load_n   (&x,  __ATOMIC_RELAXED)
#define Atomic_GET_STRICT(x)   __atomic_load_n   (&x,  __ATOMIC_ACQUIRE)
#define Atomic_INC(x)          __atomic_fetch_add(&x,1,__ATOMIC_RELAXED)
#define Atomic_SET(x,y)        __atomic_store_n  (&x,y,__ATOMIC_RELAXED)
#define Atomic_SET_STRICT(x,y) __atomic_store_n  (&x,y,__ATOMIC_RELEASE)
#define Atomic_ZAP(x)          __atomic_store_n  (&x,0,__ATOMIC_RELAXED)
#define Atomic_END(x)

//-----------------------------------------------------------------------------
//! Use old-style gcc builtins if they area available. The STRICT variants
//! are only effective on strict memory compliant machines (e.g. x86, SPARC).
//! This doesn't get resolved until gcc 4.7, sigh.
//-----------------------------------------------------------------------------
#elif HAVE_ATOMICS
#define Atomic(type)    type
#define Atomic_IMP "gnu-sync"
#define Atomic_BEG(x)
#define Atomic_DEC(x)              __sync_fetch_and_sub(&x, 1)
#define Atomic_GET(x)              __sync_fetch_and_or (&x, 0)
#define Atomic_GET_STRICT(x)       __sync_fetch_and_or (&x, 0)
#define Atomic_INC(x)              __sync_fetch_and_add(&x, 1)
#define Atomic_SET(x,y)        x=y,__sync_synchronize()
#define Atomic_SET_STRICT(x,y)     __sync_synchronize(),x=y,__sync_synchronize()
#define Atomic_ZAP(x)              __sync_fetch_and_and(&x, 0)
#define Atomic_END(x)

//-----------------------------------------------------------------------------
//! Use ordinary operators since the program needs to use mutexes
//-----------------------------------------------------------------------------
#else
#define NEED_ATOMIC_MUTEX 1
#define Atomic_IMP "missing"
#define Atomic(type)    type
#define Atomic_BEG(x)   pthread_mutex_lock(x)
#define Atomic_DEC(x)   x--
#define Atomic_GET(x)   x
#define Atomic_INC(x)   x++
#define Atomic_SET(x,y) x = y
#define Atomic_ZAP(x)   x = 0
#define Atomic_END(x)   pthread_mutex_unlock(x)
#endif

/******************************************************************************/
/*                           X r d S s i M u t e x                            */
/******************************************************************************/

#include <pthread.h>

class XrdSsiMutex
{
public:

inline bool TryLock() {return pthread_mutex_trylock( &cs ) == 0;}

inline void    Lock() {pthread_mutex_lock(&cs);}

inline void  UnLock() {pthread_mutex_unlock(&cs);}

enum MutexType {Simple = 0, Recursive = 1};

       XrdSsiMutex(MutexType mt=Simple)
                  {int rc;
                   if (mt == Simple) rc = pthread_mutex_init(&cs, NULL);
                      else {pthread_mutexattr_t attr;
                            if (!(rc = pthread_mutexattr_init(&attr)))
                               {pthread_mutexattr_settype(&attr,
                                                     PTHREAD_MUTEX_RECURSIVE);
                                rc = pthread_mutex_init(&cs, &attr);
                               }
                           }
                   if (rc) throw Errno2Text(rc);
                  }

      ~XrdSsiMutex() {pthread_mutex_destroy(&cs);}

protected:

pthread_mutex_t cs;

private:
const char* Errno2Text(int ecode);
};
  
/******************************************************************************/
/*                        X r d S s i M u t e x M o n                         */
/******************************************************************************/
  
class XrdSsiMutexMon
{
public:

inline void   Lock(XrdSsiMutex *mutex)
                  {if (mtx) {if (mtx != mutex) mtx->UnLock();
                                else return;
                            }
                   mutex->Lock();
                   mtx = mutex;
                  };

inline void   Lock(XrdSsiMutex &mutex) {Lock(&mutex);}

inline void   Reset() {mtx = 0;}

inline void UnLock() {if (mtx) {mtx->UnLock(); mtx = 0;}}

            XrdSsiMutexMon(XrdSsiMutex *mutex=0)
                          {if (mutex) mutex->Lock();
                           mtx =  mutex;
                          }
            XrdSsiMutexMon(XrdSsiMutex &mutex)
                          {mutex.Lock();
                           mtx = &mutex;
                          }

           ~XrdSsiMutexMon() {if (mtx) UnLock();}
private:
XrdSsiMutex *mtx;
};
#endif
