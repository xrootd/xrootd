#ifndef __SYS_PTHREAD__
#define __SYS_PTHREAD__
/******************************************************************************/
/*                                                                            */
/*                      X r d S y s P t h r e a d . h h                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cerrno>
#ifdef WIN32
#define HAVE_STRUCT_TIMESPEC 1
#endif
#include <pthread.h>
#include <signal.h>
#ifdef AIX
#include <sys/sem.h>
#else
#include <semaphore.h>
#endif

#ifdef __APPLE__
#ifndef CLOCK_REALTIME
#include <mach/clock.h>
#include <mach/mach.h>
#endif
namespace
{
  template< typename TYPE >
  void get_apple_realtime( TYPE & wait )
  {
#ifdef CLOCK_REALTIME
    clock_gettime(CLOCK_REALTIME, &wait);
#else
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    wait.tv_sec  = mts.tv_sec;
    wait.tv_nsec = mts.tv_nsec;
#endif
  }
}
#endif

#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                         X r d S y s C o n d V a r                          */
/******************************************************************************/
  
// XrdSysCondVar implements the standard POSIX-compliant condition variable.
//               Methods correspond to the equivalent pthread condvar functions.

class XrdSysCondVar
{
public:

inline void  Lock()           {pthread_mutex_lock(&cmut);}

inline void  Signal()         {if (relMutex) pthread_mutex_lock(&cmut);
                               pthread_cond_signal(&cvar);
                               if (relMutex) pthread_mutex_unlock(&cmut);
                              }

inline void  Broadcast()      {if (relMutex) pthread_mutex_lock(&cmut);
                               pthread_cond_broadcast(&cvar);
                               if (relMutex) pthread_mutex_unlock(&cmut);
                              }

inline void  UnLock()         {pthread_mutex_unlock(&cmut);}

       int   Wait();
       int   Wait(int sec);
       int   WaitMS(int msec);

      XrdSysCondVar(      int   relm=1, // 0->Caller will handle lock/unlock
                    const char *cid=0   // ID string for debugging only
                   ) {pthread_cond_init(&cvar, NULL);
                      pthread_mutex_init(&cmut, NULL);
                      relMutex = relm; condID = (cid ? cid : "unk");
                     }
     ~XrdSysCondVar() {pthread_cond_destroy(&cvar);
                       pthread_mutex_destroy(&cmut);
                      }
private:

pthread_cond_t  cvar;
pthread_mutex_t cmut;
int             relMutex;
const char     *condID;
};


/******************************************************************************/
/*                     X r d S y s C o n d V a r H e l p e r                  */
/******************************************************************************/

// XrdSysCondVarHelper is used to implement monitors with the Lock of a a condvar.
//                     Monitors are used to lock
//                     whole regions of code (e.g., a method) and automatically
//                     unlock with exiting the region (e.g., return). The
//                     methods should be self-evident.
  
class XrdSysCondVarHelper
{
public:

inline void   Lock(XrdSysCondVar *CndVar)
                  {if (cnd) {if (cnd != CndVar) cnd->UnLock();
                                else return;
                            }
                   CndVar->Lock();
                   cnd = CndVar;
                  };

inline void UnLock() {if (cnd) {cnd->UnLock(); cnd = 0;}}

            XrdSysCondVarHelper(XrdSysCondVar *CndVar=0)
                 {if (CndVar) CndVar->Lock();
                  cnd = CndVar;
                 }
            XrdSysCondVarHelper(XrdSysCondVar &CndVar)
                 {CndVar.Lock();
                  cnd = &CndVar;
                 }

           ~XrdSysCondVarHelper() {if (cnd) UnLock();}
private:
XrdSysCondVar *cnd;
};


/******************************************************************************/
/*                           X r d S y s M u t e x                            */
/******************************************************************************/

// XrdSysMutex implements the standard POSIX mutex. The methods correspond
//             to the equivalent pthread mutex functions.
  
class XrdSysMutex
{
public:
friend class XrdSysCondVar2;

inline int CondLock()
       {if (pthread_mutex_trylock( &cs )) return 0;
        return 1;
       }
#ifdef __APPLE__
inline int TimedLock( int wait_ms )
{
  struct timespec wait, cur, dur;
  get_apple_realtime(wait);
  wait.tv_sec += (wait_ms / 1000);
  wait.tv_nsec += (wait_ms % 1000) * 1000000;
  wait.tv_sec += (wait.tv_nsec / 1000000000);
  wait.tv_nsec = wait.tv_nsec % 1000000000;

  int rc;
  while( ( rc = pthread_mutex_trylock( &cs ) ) == EBUSY )
  {
    get_apple_realtime(cur);
    if( ( cur.tv_sec > wait.tv_sec ) || 
	( ( cur.tv_sec == wait.tv_sec ) && ( cur.tv_nsec >= wait.tv_nsec ) ) )
      return 0;

    dur.tv_sec  = wait.tv_sec  - cur.tv_sec;
    dur.tv_nsec = wait.tv_nsec - cur.tv_nsec;
    if( dur.tv_nsec < 0 )
    {
      --dur.tv_sec;
      dur.tv_nsec += 1000000000;
    }
    
    if( ( dur.tv_sec != 0 ) || ( dur.tv_nsec > 1000000 ) )
    {
      dur.tv_sec  = 0;
      dur.tv_nsec = 1000000;
    }

    nanosleep( &dur, 0 );
  }

  return !rc;
}
#else
inline int TimedLock(int wait_ms)
       {struct timespec wait;
        clock_gettime(CLOCK_REALTIME, &wait);
        wait.tv_sec += (wait_ms / 1000);
        wait.tv_nsec += (wait_ms % 1000) * 1000000;
        wait.tv_sec += (wait.tv_nsec / 1000000000);
        wait.tv_nsec = wait.tv_nsec % 1000000000;
        return !pthread_mutex_timedlock(&cs, &wait);
       }
#endif

inline void   Lock() {pthread_mutex_lock(&cs);}

inline void UnLock() {pthread_mutex_unlock(&cs);}

        XrdSysMutex() {pthread_mutex_init(&cs, NULL);}
       ~XrdSysMutex() {pthread_mutex_destroy(&cs);}

protected:

pthread_mutex_t cs;
};

/******************************************************************************/
/*                         X r d S y s R e c M u t e x                        */
/******************************************************************************/

// XrdSysRecMutex implements the recursive POSIX mutex. The methods correspond
//             to the equivalent pthread mutex functions.
  
class XrdSysRecMutex: public XrdSysMutex
{
public:

XrdSysRecMutex();

int InitRecMutex();
int ReInitRecMutex();

};


/******************************************************************************/
/*                     X r d S y s M u t e x H e l p e r                      */
/******************************************************************************/

// XrdSysMutexHelper us ised to implement monitors. Monitors are used to lock
//                   whole regions of code (e.g., a method) and automatically
//                   unlock with exiting the region (e.g., return). The
//                   methods should be self-evident.
  
class XrdSysMutexHelper
{
public:

inline void   Lock(XrdSysMutex *Mutex)
                  {if (mtx) {if (mtx != Mutex) mtx->UnLock();
                                else return;
                            }
                   Mutex->Lock();
                   mtx = Mutex;
                  };

inline void UnLock() {if (mtx) {mtx->UnLock(); mtx = 0;}}

            XrdSysMutexHelper(XrdSysMutex *mutex=0)
                 {if (mutex) mutex->Lock();
                  mtx = mutex;
                 }
            XrdSysMutexHelper(XrdSysMutex &mutex)
                 {mutex.Lock();
                  mtx = &mutex;
                 }

           ~XrdSysMutexHelper() {if (mtx) UnLock();}
private:
XrdSysMutex *mtx;
};
  
/******************************************************************************/
/*                        X r d S y s C o n d V a r 2                         */
/******************************************************************************/
  
// XrdSysCondVar2 implements the standard POSIX-compliant condition variable but
//                unlike XrdSysCondVar requires the caller to supply a working
//                mutex and does not handle any locking other than what is
//                defined by POSIX.

class XrdSysCondVar2
{
public:

inline void  Signal()         {pthread_cond_signal(&cvar);}

inline void  Broadcast()      {pthread_cond_broadcast(&cvar);}

inline int   Wait()           {return pthread_cond_wait(&cvar, mtxP);}
       bool  Wait(int sec)    {return WaitMS(sec*1000);}
       bool  WaitMS(int msec);

       XrdSysCondVar2(XrdSysMutex &mtx) : mtxP(&mtx.cs)
                     {pthread_cond_init(&cvar, NULL);}

      ~XrdSysCondVar2() {pthread_cond_destroy(&cvar);}

protected:

pthread_cond_t   cvar;
pthread_mutex_t *mtxP;
};

/******************************************************************************/
/*                           X r d S y s R W L o c k                          */
/******************************************************************************/

// XrdSysRWLock implements the standard POSIX wrlock mutex. The methods correspond
//             to the equivalent pthread wrlock functions.
  
class XrdSysRWLock
{
public:

inline int CondReadLock()
       {if (pthread_rwlock_tryrdlock( &lock )) return 0;
        return 1;
       }
inline int CondWriteLock()
       {if (pthread_rwlock_trywrlock( &lock )) return 0;
        return 1;
       }

inline void  ReadLock() {pthread_rwlock_rdlock(&lock);}
inline void  WriteLock() {pthread_rwlock_wrlock(&lock);}

inline void ReadLock( int &status ) {status = pthread_rwlock_rdlock(&lock);}
inline void WriteLock( int &status ) {status = pthread_rwlock_wrlock(&lock);}

inline void UnLock() {pthread_rwlock_unlock(&lock);}

enum PrefType {prefWR=1};

        XrdSysRWLock(PrefType ptype)
                    {
#if defined(__linux__) && (defined(__GLIBC__) || defined(__UCLIBC__))
                     pthread_rwlockattr_t attr;
                     pthread_rwlockattr_setkind_np(&attr,
                             PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
                     pthread_rwlock_init(&lock, &attr);
#else
                     pthread_rwlock_init(&lock, NULL);
#endif
                    }

        XrdSysRWLock() {pthread_rwlock_init(&lock, NULL);}
       ~XrdSysRWLock() {pthread_rwlock_destroy(&lock);}

inline void ReInitialize(PrefType ptype)
{
  pthread_rwlock_destroy(&lock);
#if defined(__linux__) && (defined(__GLIBC__) || defined(__UCLIBC__))
  pthread_rwlockattr_t attr;
  pthread_rwlockattr_setkind_np(&attr,
                     PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  pthread_rwlock_init(&lock, &attr);
#else
  pthread_rwlock_init(&lock, NULL);
#endif
}

inline void ReInitialize()
{
  pthread_rwlock_destroy(&lock);
  pthread_rwlock_init(&lock, NULL);
}

protected:

pthread_rwlock_t lock;
};

/******************************************************************************/
/*                     X r d S y s W R L o c k H e l p e r                    */
/******************************************************************************/

// XrdSysWRLockHelper : helper class for XrdSysRWLock
  
class XrdSysRWLockHelper
{
public:

inline void   Lock(XrdSysRWLock *lock, bool rd = 1)
                  {if (lck) {if (lck != lock) lck->UnLock();
                                else return;
                            }
                   if (rd) lock->ReadLock();
                      else lock->WriteLock();
                   lck = lock;
                  };

inline void UnLock() {if (lck) {lck->UnLock(); lck = 0;}}

            XrdSysRWLockHelper(XrdSysRWLock *l=0, bool rd = 1)
                 { if (l) {if (rd) l->ReadLock();
                              else l->WriteLock();
                          }
                   lck = l;
                 }
            XrdSysRWLockHelper(XrdSysRWLock &l, bool rd = 1)
                 { if (rd) l.ReadLock();
                      else l.WriteLock();
                   lck = &l;
                 }

           ~XrdSysRWLockHelper() {if (lck) UnLock();}
private:
XrdSysRWLock *lck;
};

/******************************************************************************/
/*                      X r d S y s F u s e d M u t e x                       */
/******************************************************************************/

class XrdSysFusedMutex
{
public:

inline void  Lock()      {isRW ? rwLok->WriteLock() : mutex->Lock();}

inline void  ReadLock()  {isRW ? rwLok->ReadLock()  : mutex->Lock();}

inline void  WriteLock() {isRW ? rwLok->WriteLock() : mutex->Lock();}

inline void  UnLock()    {isRW ? rwLok->UnLock()    : mutex->UnLock();}

             XrdSysFusedMutex(XrdSysRWLock &mtx)
                             : rwLok(&mtx), isRW(true) {}

             XrdSysFusedMutex(XrdSysMutex  &mtx)
                             : mutex(&mtx), isRW(false) {}

            ~XrdSysFusedMutex() {}
private:

union {XrdSysRWLock *rwLok; XrdSysMutex *mutex;};
bool  isRW;
};
  
/******************************************************************************/
/*                       X r d S y s S e m a p h o r e                        */
/******************************************************************************/

// XrdSysSemaphore implements the classic counting semaphore. The methods
//                 should be self-evident. Note that on certain platforms
//                 semaphores need to be implemented based on condition
//                 variables since no native implementation is available.
  
#if defined(__APPLE__) || defined(__GNU__)
class XrdSysSemaphore
{
public:

       int  CondWait();

       void Post();

       void Wait();

static void CleanUp(void *semVar);

  XrdSysSemaphore(int semval=1,const char *cid=0) : semVar(0, cid)
                                  {semVal = semval; semWait = 0;}
 ~XrdSysSemaphore() {}

private:

XrdSysCondVar semVar;
int           semVal;
int           semWait;
};

#else

class XrdSysSemaphore
{
public:

inline int  CondWait()
       {while(sem_trywait( &h_semaphore ))
             {if (errno == EAGAIN) return 0;
              if (errno != EINTR) { throw "sem_CondWait() failed";}
             }
        return 1;
       }

inline void Post() {if (sem_post(&h_semaphore))
                       {throw "sem_post() failed";}
                   }

inline void Wait() {while (sem_wait(&h_semaphore))
                          {if (EINTR != errno) 
                              {throw "sem_wait() failed";}
                          }
                   }

  XrdSysSemaphore(int semval=1, const char * =0)
                               {if (sem_init(&h_semaphore, 0, semval))
                                   {throw "sem_init() failed";}
                               }
 ~XrdSysSemaphore() {if (sem_destroy(&h_semaphore))
                        {abort();}
                    }

private:

sem_t h_semaphore;
};
#endif

/******************************************************************************/
/*                          X r d S y s T h r e a d                           */
/******************************************************************************/
  
// The C++ standard makes it impossible to link extern "C" methods with C++
// methods. Thus, making a full thread object is nearly impossible. So, this
// object is used as the thread manager. Since it is static for all intense
// and purposes, one does not need to create an instance of it.
//

// Options to Run()
//
// BIND creates threads that are bound to a kernel thread.
//
#define XRDSYSTHREAD_BIND 0x001

// HOLD creates a thread that needs to be joined to get its ending value.
//      Otherwise, a detached thread is created.
//
#define XRDSYSTHREAD_HOLD 0x002

class XrdSysThread
{
public:

static int          Cancel(pthread_t tid) {return pthread_cancel(tid);}

static int          Detach(pthread_t tid) {return pthread_detach(tid);}


static  int  SetCancelOff() {
      return pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
 };

static  int  Join(pthread_t tid, void **ret) {
   return pthread_join(tid, ret);
 };

static  int  SetCancelOn() {
      return pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
 };

static  int  SetCancelAsynchronous() {
      return pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
 };

static int  SetCancelDeferred() {
      return pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, 0);
 };

static void  CancelPoint() {
      pthread_testcancel();
 };


static pthread_t    ID(void)              {return pthread_self();}

static int          Kill(pthread_t tid)   {return pthread_cancel(tid);}

static unsigned long Num(void);

static int          Run(pthread_t *, void *(*proc)(void *), void *arg, 
                        int opts=0, const char *desc = 0);

static int          Same(pthread_t t1, pthread_t t2)
                        {return pthread_equal(t1, t2);}

static void         setDebug(XrdSysError *erp) {eDest = erp;}

static void         setStackSize(size_t stsz, bool force=false);

static int          Signal(pthread_t tid, int snum)
                       {return pthread_kill(tid, snum);}
 
static int          Wait(pthread_t tid);

                    XrdSysThread() {}
                   ~XrdSysThread() {}

private:
static XrdSysError  *eDest;
static size_t        stackSize;
};
#endif
