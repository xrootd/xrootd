#ifndef __OOUC_PTHREAD__
#define __OOUC_PTHREAD__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c P t h r e a d . h h                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//        $Id$

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#ifdef AIX
#include <sys/sem.h>
#else
#include <semaphore.h>
#endif

#include "XrdOuc/XrdOucError.hh"

/******************************************************************************/
/*                         X r d O u c C o n d V a r                          */
/******************************************************************************/
  
// XrdOucConVar implements the standard POSIX-compliant condition variable.
//              Methods correspond to the equivalent pthread condvar functions.

class XrdOucCondVar
{
public:

inline void  Lock()           {pthread_mutex_lock(&cmut);}

inline void  Signal()         {if (relMutex) pthread_mutex_lock(&cmut);
                               pthread_cond_signal(&cvar);
                               if (relMutex) pthread_mutex_unlock(&cmut);
                              }

inline void  UnLock()         {pthread_mutex_unlock(&cmut);}

       int   Wait();
       int   Wait(int sec);
       int   WaitMS(int msec);

      XrdOucCondVar(      int   relm=1, // 0->Caller will handle lock/unlock
                    const char *cid=0   // ID string for debugging only
                   ) {pthread_cond_init(&cvar, NULL);
                      pthread_mutex_init(&cmut, NULL);
                      relMutex = relm; condID = (cid ? cid : "unk");
                     }
     ~XrdOucCondVar() {pthread_cond_destroy(&cvar);
                       pthread_mutex_destroy(&cmut);
                      }
private:

pthread_cond_t  cvar;
pthread_mutex_t cmut;
int             relMutex;
const char     *condID;
};

/******************************************************************************/
/*                           X r d O u c M u t e x                            */
/******************************************************************************/

// XrdOucMutex implements the standard POSIX mutex. The methods correspond
//             to the equivalent pthread mutex functions.
  
class XrdOucMutex
{
public:

inline int CondLock()
       {if (pthread_mutex_trylock( &cs )) return 0;
        return 1;
       }

inline void   Lock() {pthread_mutex_lock(&cs);}

inline void UnLock() {pthread_mutex_unlock(&cs);}

        XrdOucMutex() {pthread_mutex_init(&cs, NULL);}
       ~XrdOucMutex() {pthread_mutex_destroy(&cs);}

private:

pthread_mutex_t cs;
};

/******************************************************************************/
/*                     X r d O u c M u t e x H e l p e r                      */
/******************************************************************************/

// XrdOucMutexHelper us ised to implement monitors. Monitors are used to lock
//                   whole regions of code (e.g., a method) and automatically
//                   unlock with exiting the region (e.g., return). The
//                   methods should be self-evident.
  
class XrdOucMutexHelper
{
public:

inline void   Lock(XrdOucMutex *Mutex)
                  {if (mtx) 
                      if (mtx != Mutex) mtx->UnLock();
                         else return;
                   Mutex->Lock();
                   mtx = Mutex;
                  };

inline void UnLock() {if (mtx) {mtx->UnLock(); mtx = 0;}}

            XrdOucMutexHelper(XrdOucMutex *mutex=0)
                 {if (mutex) Lock(mutex);
                     else mtx = 0;
                 }
           ~XrdOucMutexHelper() {if (mtx) UnLock();}
private:
XrdOucMutex *mtx;
};

/******************************************************************************/
/*                       X r d O u c S e m a p h o r e                        */
/******************************************************************************/

// XrdOucSemaphore implements the classic counting semaphore. The methods
//                 should be self-evident. Note that on certain platforms
//                 semaphores need to be implemented based on condition
//                 variables since no native implementation is available.
  
#ifdef __macos__
class XrdOucSemaphore
{
public:

       int  CondWait();

       void Post();

       void Wait();

  XrdOucSemaphore(int semval=1,const char *cid=0) : semVar(0, cid)
                                  {semVal = semval; semWait = 0;}
 ~XrdOucSemaphore() {}

private:

XrdOucCondVar semVar;
int           semVal;
int           semWait;
};

#else

class XrdOucSemaphore
{
public:

inline int  CondWait()
       {if (sem_trywait( &h_semaphore ))
           if (errno == EBUSY) return 0;
               else { throw "sem_CondWait() failed";}
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

  XrdOucSemaphore(int semval=1, const char *cid=0)
                               {if (sem_init(&h_semaphore, 0, semval))
                                   {throw "sem_init() failed";}
                               }
 ~XrdOucSemaphore() {if (sem_destroy(&h_semaphore))
                       {throw "sem_destroy() failed";}
                   }

private:

sem_t h_semaphore;
};
#endif

/******************************************************************************/
/*                          X r d O u c T h r e a d                           */
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
#define XRDOUCTHREAD_BIND 0x001

// HOLD creates a thread that needs to be joined to get its ending value.
//      Otherwise, a detached thread is created.
//
#define XRDOUCTHREAD_HOLD 0x002

class XrdOucThread
{
public:

static int          Cancel(pthread_t tid) {return pthread_cancel(tid);}

static int          Detach(pthread_t tid) {return pthread_detach(tid);}

static pthread_t    ID(void)              {return pthread_self();}

static int          Kill(pthread_t tid)   {return pthread_cancel(tid);}

static unsigned long Num(void)
                       {if (!initDone) doInit();
                        return (unsigned long)pthread_getspecific(threadNumkey);
                       }

static int          Run(pthread_t *, void *(*proc)(void *), void *arg, 
                        int opts=0, const char *desc = 0);

static void         setDebug(XrdOucError *erp) {eDest = erp;}

static void         setStackSize(size_t stsz) {stackSize = stsz;}

static int          Signal(pthread_t tid, int snum)
                       {return pthread_kill(tid, snum);}
 
static int          Wait(pthread_t tid);

                    XrdOucThread() {}
                   ~XrdOucThread() {}

private:
static void          doInit(void);
static XrdOucError  *eDest;
static pthread_key_t threadNumkey;
static size_t        stackSize;
static int           initDone;
};
#endif
