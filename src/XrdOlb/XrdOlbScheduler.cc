/******************************************************************************/
/*                                                                            */
/*                    X r d O l b S c h e d u l e r . c c                     */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOlbSchedulerCVSID = "$Id$";
  
#include <sys/types.h>

#include "XrdNet/XrdNetLink.hh"
#include "XrdOlb/XrdOlbScheduler.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOuc/XrdOucError.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdOucTrace   XrdOlbTrace;

extern XrdOucError   XrdOlbSay;

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdOlbStartWorking(void *carg)
      {XrdOlbScheduler *sp = (XrdOlbScheduler *)carg;
       return sp->WorkIt();
      }

void *XrdOlbStartTSched(void *carg)
      {XrdOlbScheduler *sp = (XrdOlbScheduler *)carg;
       sp->TimeSched();
       return (void *)0;
      }

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOlbScheduler::XrdOlbScheduler()
{
    int retc;
    pthread_t tid;

    min_Workers = 16;
    max_Workers = 64;
    num_Workers =  0;
    TimerQueue  = 0;
    WorkQueue   = 0;
    WorkQLast   = 0;

// Start a time based scheduler
//
   if ((retc = XrdOucThread::Run(&tid, XrdOlbStartTSched, (void *)this,
                                 0, "Time scheduler")))
      XrdOlbSay.Emsg("Scheduler", retc, "create time scheduler thread");
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOlbScheduler::~XrdOlbScheduler()
{
}
 
/******************************************************************************/
/*                              S c h e d u l e                               */
/******************************************************************************/
  
void XrdOlbScheduler::Schedule(XrdOlbJob *jp)
{

// Check if we have enough workers available
//
   jp->NextJob = 0;
   WorkMutex.Lock();
   if (!num_Workers || (!WorkQueue && num_Workers < max_Workers)) hireWorker();

// Place the request on the queue and broadcast it
//
   if (WorkQLast) WorkQLast->NextJob = jp;
      else        WorkQueue          = jp;
   WorkQLast = jp;
   WorkMutex.UnLock();
   WorkAvail.Post();
}

/******************************************************************************/
 
void XrdOlbScheduler::Schedule(XrdOlbJob *jp, time_t atime)
{
   EPNAME("Schedule")
   XrdOlbJob *pp = 0, *p;

// Lock the queue
//
   DEBUG("scheduling " <<jp->Comment <<" in " <<atime-time(0) <<" seconds");
   jp->SchedTime = atime;
   TimerMutex.Lock();

// Find the insertion point for the work element
//
   p = TimerQueue;
   while(p && p->SchedTime <= atime) {pp = p; p = p->NextJob;}

// Insert the job element
//
   jp->NextJob = p;
   if (pp)  pp->NextJob = jp;
      else {TimerQueue = jp; TimerRings.Signal();}

// All done
//
   TimerMutex.UnLock();
}

/******************************************************************************/
/*                            s e t W o r k e r s                             */
/******************************************************************************/
  
void XrdOlbScheduler::setWorkers(int minw, int maxw)
{
   EPNAME("setWorkers")

// Lock the data area
//
   WorkMutex.Lock();

// Set the values
//
   min_Workers = minw;
   max_Workers = maxw;

// Unlock the data area
//
   WorkMutex.UnLock();

// Debug the info
//
   DEBUG("New min_Workers=" <<minw <<" max_Workers=" <<maxw);
}

/******************************************************************************/
/*                             T i m e S c h e d                              */
/******************************************************************************/
  
void XrdOlbScheduler::TimeSched()
{
   XrdOlbJob *jp;
   int wtime;

// Continuous loop until we find some work here
//
   do {TimerMutex.Lock();
       if (TimerQueue) wtime = TimerQueue->SchedTime-time(0);
          else wtime = 60*60;
       if (wtime > 0)
          {TimerMutex.UnLock();
           TimerRings.Wait(wtime);
          } else {
           jp = TimerQueue;
           TimerQueue = jp->NextJob;
           TimerMutex.UnLock();
           Schedule(jp);
          }
       } while(1);
}
 
/******************************************************************************/
/*                                W o r k I t                                 */
/******************************************************************************/
  
void *XrdOlbScheduler::WorkIt()
{
   EPNAME("WorkIt");
   XrdOlbJob *jp;

// Wait for work
//
   do {WorkAvail.Wait();
       WorkMutex.Lock();
       if ((jp = WorkQueue))
          {if (!(WorkQueue = jp->NextJob)) WorkQLast = 0;
           WorkMutex.UnLock();
           DEBUG("running " <<jp->Comment);
           if (jp->DoIt() == 0) delete jp;
          } else WorkMutex.UnLock();
      } while(1);
   return (void *)0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                           h i r e   W o r k e r                            */
/******************************************************************************/
  
// Entered with WorkMutex locked!

void XrdOlbScheduler::hireWorker()
{
   EPNAME("hireWorker")
   pthread_t tid;
   int retc;

// Start a new thread
//
   if ((retc = XrdOucThread::Run(&tid, XrdOlbStartWorking, (void *)this,
                                 0, "Worker")))
      XrdOlbSay.Emsg("Scheduler", retc, "create worker thread");
      else {num_Workers++;
            DEBUG("Now have " <<num_Workers <<" workers");
           }
}
