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
#ifdef AIX
#include <sys/sem.h>
#else
#include <semaphore.h>
#endif

class XrdOucCondVar
{
public:

void  Signal()         {pthread_mutex_lock(&cmut);
                        pthread_cond_signal(&cvar);
                        pthread_mutex_unlock(&cmut);
                       }

int   Wait(int sec);
int   WaitMS(int msec);

      XrdOucCondVar() {pthread_cond_init(&cvar, NULL);
                      pthread_mutex_init(&cmut, NULL);
                     }
     ~XrdOucCondVar() {pthread_cond_destroy(&cvar);
                      pthread_mutex_destroy(&cmut);
                     }
private:

pthread_cond_t  cvar;
pthread_mutex_t cmut;
struct timespec tval;
};

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

  XrdOucSemaphore(int semval=1) {if (sem_init(&h_semaphore, 0, semval))
                                   {throw "sem_init() failed";}
                               }
 ~XrdOucSemaphore() {if (sem_destroy(&h_semaphore))
                       {throw "sem_destroy() failed";}
                   }

private:

sem_t h_semaphore;
};


extern "C"
{
extern int       XrdOucThread_Cancel(pthread_t tid);
extern int       XrdOucThread_Detach(pthread_t tid);
extern pthread_t XrdOucThread_ID(void);
extern int       XrdOucThread_Kill(pthread_t tid);
extern int XrdOucThread_Run(pthread_t *, void *(*proc)(void *), void *arg);
extern int XrdOucThread_Sys(pthread_t *, void *(*proc)(void *), void *arg);
extern int XrdOucThread_Signal(pthread_t tid, int snum);
extern int XrdOucThread_Start(pthread_t *, void *(*proc)(void *), void *arg);
extern int XrdOucThread_Wait(pthread_t tid);
}

// The C++ standard makes it impossible to link extern "C" methods with C++
// methods. Thus, making a full thread object is nearly impossible. So, this
// object merely ties a thread id to a set of methods. One needs to externally
// call XrdOucThread_Start() or Run() to get a thread id and then assigned it
// to a thread object.
//

class XrdOucThread
{
public:

void  Attach(pthread_t xid, int isdetached=0)
               {if (tactive > 1)  XrdOucThread_Detach(tid);
                tid = xid;
                tactive = (isdetached ? 1 : 2);
               }

void  Cancel() {if (tactive)     {XrdOucThread_Cancel(tid); tactive = 0;}}

void  Detach() {if (tactive > 1) {XrdOucThread_Detach(tid); tactive = 1;}}

void  Kill()   {if (tactive)     {XrdOucThread_Kill(tid);   tactive = 0;}}

void  Signal(int snum) {if (tactive) XrdOucThread_Signal(tid, snum);}

int   Wait() {if (tactive > 1) return XrdOucThread_Wait(tid); return -1;}

      XrdOucThread() {tactive = 0;}
     ~XrdOucThread() {Detach();}

pthread_t tid;
int tactive;
};
#endif
