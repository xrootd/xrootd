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
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#include "XrdOuc/XrdOucPthread.hh"

extern "C"
{
int XrdOucThread_Cancel(pthread_t tid)
    {return pthread_cancel(tid);}

int XrdOucThread_Detach(pthread_t tid)
    {return pthread_detach(tid);}

pthread_t XrdOucThread_ID()
    {return pthread_self();}

int XrdOucThread_Kill(pthread_t tid)
    {
     return pthread_kill(tid, SIGKILL);
    }

int XrdOucThread_Signal(pthread_t tid, int snum)
    {
     return pthread_kill(tid, snum);
    }

int XrdOucThread_Start(pthread_t *tid, void *(*proc)(void *), void *arg)
     {return pthread_create(tid, NULL, proc, arg);}

int  XrdOucThread_Run(pthread_t *tid, void *(*proc)(void *), void *arg)
     {int rc;
      if ((rc = XrdOucThread_Start(tid, proc, arg))) return rc;
      return    XrdOucThread_Detach(*tid);
     }

int  XrdOucThread_Sys(pthread_t *tid, void *(*proc)(void *), void *arg)
     {int rc;
      pthread_attr_t tattr;
      pthread_attr_init(&tattr);
      pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
      pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
      if ((rc = pthread_create(tid, &tattr, proc, arg))) return rc;
      return 0;
     }

int XrdOucThread_Wait(pthread_t tid)
    {int retc, *tstat;
     if ((retc = pthread_join(tid, (void **)&tstat))) return retc;
     return *tstat;
    }
}
  
/******************************************************************************/
/*                       C o n d V a r   M e t h o d s                        */
/******************************************************************************/
  
int XrdOucCondVar::Wait(int sec)
{
 int retc;

// Simply adjust the time in seconds
//
   tval.tv_sec  = time(0) + sec;
   tval.tv_nsec = 0;

// Wait for the condition or timeout
//
   pthread_mutex_lock(&cmut);
   retc = pthread_cond_timedwait(&cvar, &cmut, &tval);
   pthread_mutex_unlock(&cmut);
   return retc == ETIMEDOUT;
}

int XrdOucCondVar::WaitMS(int msec)
{
 int sec, retc, usec;
 struct timeval tnow;

// Adjust millseconds
//
    if (msec < 1000) sec = 0;
       else {sec = msec / 1000; msec = msec % 1000;}
    usec = msec * 1000;

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
    pthread_mutex_lock(&cmut);
    retc = pthread_cond_timedwait(&cvar, &cmut, &tval);
    pthread_mutex_unlock(&cmut);
    return retc == ETIMEDOUT;
}
