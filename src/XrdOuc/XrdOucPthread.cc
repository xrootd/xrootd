/******************************************************************************/
/*                                                                            */
/*                      X r d O u c P t h r e a d . c c                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//        $Id$

const char *XrdOucPthreadCVSID = "$Id$";
 
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucPthread.hh"

/******************************************************************************/
/*                         L o c a l   S t r u c t s                          */
/******************************************************************************/

struct XrdOucThreadArgs
       {
        pthread_key_t numKey;
        XrdOucError  *eDest;
        const char   *tDesc;
        void         *(*proc)(void *);
        void         *arg;

        XrdOucThreadArgs(pthread_key_t nk, XrdOucError *ed, const char *td,
                         void *(*p)(void *), void *a)
                        {numKey=nk; eDest=ed; tDesc=td, proc=p; arg=a;}
       ~XrdOucThreadArgs() {}
       };

/******************************************************************************/
/*                           G l o b a l   D a t a                            */
/******************************************************************************/
  
pthread_key_t XrdOucThread::threadNumkey;

XrdOucError  *XrdOucThread::eDest     = 0;

size_t        XrdOucThread::stackSize = 0;

int           XrdOucThread::initDone  = 0;

/******************************************************************************/
/*             T h r e a d   I n t e r f a c e   P r o g r a m s              */
/******************************************************************************/
  
extern "C"
{
void *XrdOucThread_Xeq(void *myargs)
{
   XrdOucThreadArgs *ap = (XrdOucThreadArgs *)myargs;
   unsigned long myNum;
   void *retc;

#if   defined(__linux__)
   myNum = static_cast<unsigned int>(getpid());
#elif defined(__sun)
   myNum = static_cast<unsigned int>(pthread_self());
#elif defined(__macos__)
   myNum = static_cast<unsigned int>(pthread_mach_thread_np(pthread_self()));
#else
   static XrdOucMutex   numMutex;
   static unsigned long threadNum = 1;
   numMutex.Lock(); threadNum++; myNum = threadNum; numMutex.UnLock();
#endif

   pthread_setspecific(ap->numKey, (const void *)myNum);
   if (ap->eDest && ap->tDesc)
      ap->eDest->Emsg("Xeq", ap->tDesc, "thread started");
   retc = ap->proc(ap->arg);
   delete ap;
   return retc;
}
}
  
/******************************************************************************/
/*                         X r d O u c C o n d V a r                          */
/******************************************************************************/
/******************************************************************************/
/*                                  W a i t                                   */
/******************************************************************************/
  
int XrdOucCondVar::Wait()
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
  
int XrdOucCondVar::Wait(int sec)
{
 struct timespec tval;
 int retc;

// Get the mutex before calculating the time
//
   if (relMutex) Lock();

// Simply adjust the time in seconds
//
   tval.tv_sec  = time(0) + sec;
   tval.tv_nsec = 0;

// Wait for the condition or timeout
//
   retc = pthread_cond_timedwait(&cvar, &cmut, &tval);
   if (relMutex) UnLock();
   return retc == ETIMEDOUT;
}

/******************************************************************************/
/*                                W a i t M S                                 */
/******************************************************************************/
  
int XrdOucCondVar::WaitMS(int msec)
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
   if (tval.tv_nsec > 1000000)
      {tval.tv_sec += tval.tv_nsec / 1000000;
       tval.tv_nsec = tval.tv_nsec % 1000000;
      }
   tval.tv_nsec *= 1000;


// Now wait for the condition or timeout
//
   retc = pthread_cond_timedwait(&cvar, &cmut, &tval);
   if (relMutex) UnLock();
   return retc == ETIMEDOUT;
}
 
/******************************************************************************/
/*                       X r d O u c S e m a p h o r e                        */
/******************************************************************************/
/******************************************************************************/
/*                              C o n d W a i t                               */
/******************************************************************************/
  
#ifdef __macos__

int XrdOucSemaphore::CondWait()
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
  
void XrdOucSemaphore::Post()
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
  
void XrdOucSemaphore::Wait()
{

// Wait until the sempahore value is positive. This will not be starvation
// free is the OS implements an unfair mutex;
//
   semVar.Lock();
   if (semVal < 1 || semWait)
      while(semVal < 1)
           {semWait++;
            semVar.Wait();
            semWait--;
           }

// Decrement the semaphore value and return
//
   semVal--;
   semVar.UnLock();
}
#endif
 
/******************************************************************************/
/*                        T h r e a d   M e t h o d s                         */
/******************************************************************************/
/******************************************************************************/
/*                                d o I n i t                                 */
/******************************************************************************/
  
void XrdOucThread::doInit()
{
   static XrdOucMutex initMutex;

   initMutex.Lock();
   if (!initDone)
      {pthread_key_create(&threadNumkey, 0);
       pthread_setspecific(threadNumkey, (const void *)1);
       initDone = 1;
      }
   initMutex.UnLock();
}
  
/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/

int XrdOucThread::Run(pthread_t *tid, void *(*proc)(void *), void *arg, 
                      int opts, const char *tDesc)
{
   pthread_attr_t tattr;
   XrdOucThreadArgs *myargs;

   if (!initDone) doInit();
   myargs = new XrdOucThreadArgs(threadNumkey, eDest, tDesc, proc, arg);

   pthread_attr_init(&tattr);
   if (  opts & XRDOUCTHREAD_BIND)
      pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
   if (!(opts & XRDOUCTHREAD_HOLD))
      pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
   if (stackSize)
       pthread_attr_setstacksize(&tattr, stackSize);
   return pthread_create(tid, &tattr, XrdOucThread_Xeq, (void *)myargs);
}

/******************************************************************************/
/*                                  W a i t                                   */
/******************************************************************************/
  
int XrdOucThread::Wait(pthread_t tid)
{
   int retc, *tstat;
   if ((retc = pthread_join(tid, (void **)&tstat))) return retc;
   return *tstat;
}



/******************************************************************************/
/*                         X r d O u c R e c M u t e x                        */
/******************************************************************************/

XrdOucRecMutex::XrdOucRecMutex() {

   int rc;
   pthread_mutexattr_t attr;

   rc = pthread_mutexattr_init(&attr);

   if (!rc) {
      rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      if (!rc)
	 pthread_mutex_init(&cs, &attr);
   }

   pthread_mutexattr_destroy(&attr);

 }
