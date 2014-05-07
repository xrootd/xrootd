/******************************************************************************/
/*                                                                            */
/*                      X r d S y s P t h r e a d . c c                       */
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
 
#include <errno.h>
#include <pthread.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#undef ETIMEDOUT       // Make sure that the definition from Winsock2.h is used ... 
#include <Winsock2.h>
#include <time.h>
#include "XrdSys/XrdWin32.hh"
#endif
#include <sys/types.h>
#if   defined(__linux__)
#include <sys/syscall.h>
#endif

#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         L o c a l   S t r u c t s                          */
/******************************************************************************/

struct XrdSysThreadArgs
       {
        XrdSysError  *eDest;
        const char   *tDesc;
        void         *(*proc)(void *);
        void         *arg;

        XrdSysThreadArgs(XrdSysError *ed, const char *td,
                         void *(*p)(void *), void *a)
                        : eDest(ed), tDesc(td), proc(p), arg(a) {}
       ~XrdSysThreadArgs() {}
       };

/******************************************************************************/
/*                           G l o b a l   D a t a                            */
/******************************************************************************/

XrdSysError  *XrdSysThread::eDest     = 0;

size_t        XrdSysThread::stackSize = 0;

/******************************************************************************/
/*             T h r e a d   I n t e r f a c e   P r o g r a m s              */
/******************************************************************************/
  
extern "C"
{
void *XrdSysThread_Xeq(void *myargs)
{
   XrdSysThreadArgs *ap = (XrdSysThreadArgs *)myargs;
   void *retc;

   if (ap->eDest && ap->tDesc)
      ap->eDest->Emsg("Xeq", ap->tDesc, "thread started");
   retc = ap->proc(ap->arg);
   delete ap;
   return retc;
}
}
  
/******************************************************************************/
/*                         X r d S y s C o n d V a r                          */
/******************************************************************************/
/******************************************************************************/
/*                                  W a i t                                   */
/******************************************************************************/
  
int XrdSysCondVar::Wait()
{
 int retc;

// Wait for the condition
//
   if (relMutex) Lock();
   retc = pthread_cond_wait(&cvar, &cmut);
   if (relMutex) UnLock();
   return retc;
}

/******************************************************************************/
  
int XrdSysCondVar::Wait(int sec) {return WaitMS(sec*1000);}

/******************************************************************************/
/*                                W a i t M S                                 */
/******************************************************************************/
  
int XrdSysCondVar::WaitMS(int msec)
{
 int sec, retc, usec;
 struct timeval tnow;
 struct timespec tval;

// Adjust millseconds
//
   if (msec < 1000) sec = 0;
      else {sec = msec / 1000; msec = msec % 1000;}
   usec = msec * 1000;

// Get the mutex before getting the time
//
   if (relMutex) Lock();

// Get current time of day
//
   gettimeofday(&tnow, 0);

// Add the second and microseconds
//
   tval.tv_sec  = tnow.tv_sec  +  sec;
   tval.tv_nsec = tnow.tv_usec + usec;
   if (tval.tv_nsec >= 1000000)
      {tval.tv_sec += tval.tv_nsec / 1000000;
       tval.tv_nsec = tval.tv_nsec % 1000000;
      }
   tval.tv_nsec *= 1000;


// Now wait for the condition or timeout
//
   do {retc = pthread_cond_timedwait(&cvar, &cmut, &tval);}
   while (retc && (retc == EINTR));

   if (relMutex) UnLock();

// Determine how to return
//
   if (retc && retc != ETIMEDOUT) {throw "cond_timedwait() failed";}
   return retc == ETIMEDOUT;
}
 
/******************************************************************************/
/*                       X r d S y s S e m a p h o r e                        */
/******************************************************************************/
/******************************************************************************/
/*                              C o n d W a i t                               */
/******************************************************************************/
  
#ifdef __APPLE__

int XrdSysSemaphore::CondWait()
{
   int rc;

// Get the semaphore only we can get it without waiting
//
   semVar.Lock();
   if ((rc = (semVal > 0) && !semWait)) semVal--;
   semVar.UnLock();
   return rc;
}

/******************************************************************************/
/*                                  P o s t                                   */
/******************************************************************************/
  
void XrdSysSemaphore::Post()
{
// Add one to the semaphore counter. If we the value is > 0 and there is a
// thread waiting for the sempagore, signal it to get the semaphore.
//
   semVar.Lock();
   semVal++;
   if (semVal && semWait) semVar.Signal();
   semVar.UnLock();
}

/******************************************************************************/
/*                                  W a i t                                   */
/******************************************************************************/
  
void XrdSysSemaphore::Wait()
{

// Wait until the semaphore value is positive. This will not be starvation
// free if the OS implements an unfair mutex.
// Adding a cleanup handler to the stack here enables threads using this OSX
// semaphore to be canceled (which is rare). A scoped lock won't work here
// because OSX is broken and doesn't call destructors properly.
//
   semVar.Lock();
   pthread_cleanup_push(&XrdSysSemaphore::CleanUp, (void *) &semVar);
   if (semVal < 1 || semWait)
      while(semVal < 1)
           {semWait++;
            semVar.Wait();
            semWait--;
           }

// Decrement the semaphore value, unlock the underlying cond var and return
//
   semVal--;
   pthread_cleanup_pop(1);
}

/******************************************************************************/
/*                               C l e a n U p                                */
/******************************************************************************/

void XrdSysSemaphore::CleanUp(void *semVar)
{
  XrdSysCondVar *sv = (XrdSysCondVar *) semVar;
  sv->UnLock();
}
#endif
 
/******************************************************************************/
/*                        T h r e a d   M e t h o d s                         */
/******************************************************************************/
/******************************************************************************/
/*                                   N u m                                    */
/******************************************************************************/

unsigned long XrdSysThread::Num()
{
#if   defined(__linux__)
   return static_cast<unsigned long>(syscall(SYS_gettid));
#elif defined(__solaris__)
   return static_cast<unsigned long>(pthread_self());
#elif defined(__APPLE__)
   return static_cast<unsigned long>(pthread_mach_thread_np(pthread_self()));
#else
   return static_cast<unsigned long>(getpid());
#endif
}
  
/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/

int XrdSysThread::Run(pthread_t *tid, void *(*proc)(void *), void *arg, 
                      int opts, const char *tDesc)
{
   pthread_attr_t tattr;
   XrdSysThreadArgs *myargs;

   myargs = new XrdSysThreadArgs(eDest, tDesc, proc, arg);

   pthread_attr_init(&tattr);
   if (  opts & XRDSYSTHREAD_BIND)
      pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
   if (!(opts & XRDSYSTHREAD_HOLD))
      pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
   if (stackSize)
       pthread_attr_setstacksize(&tattr, stackSize);
   return pthread_create(tid, &tattr, XrdSysThread_Xeq,
   			 static_cast<void *>(myargs));
}

/******************************************************************************/
/*                                  W a i t                                   */
/******************************************************************************/
  
int XrdSysThread::Wait(pthread_t tid)
{
   int retc, *tstat;
   if ((retc = pthread_join(tid, reinterpret_cast<void **>(&tstat)))) return retc;
   return *tstat;
}



/******************************************************************************/
/*                         X r d S y s R e c M u t e x                        */
/******************************************************************************/
XrdSysRecMutex::XrdSysRecMutex()
{
  InitRecMutex();
}

int XrdSysRecMutex::InitRecMutex()
{
  int rc;
  pthread_mutexattr_t attr;

  rc = pthread_mutexattr_init( &attr );

  if( !rc )
  {
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
    pthread_mutex_destroy( &cs );
    rc = pthread_mutex_init( &cs, &attr );
  }

  pthread_mutexattr_destroy(&attr);
  return rc;
}

int XrdSysRecMutex::ReInitRecMutex()
{
  pthread_mutex_destroy( &cs );
  return InitRecMutex();
}
