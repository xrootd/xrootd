#ifndef ___OLB_SCHED_H___
#define ___OLB_SCHED_H___
/******************************************************************************/
/*                                                                            */
/*                    X r d O l b S c h e d u l e r . h h                     */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$
  
#include "XrdOuc/XrdOucPthread.hh"

/******************************************************************************/
/*                        C l a s s   o o l b _ J o b                         */
/******************************************************************************/

class XrdOlbJob
{
friend class XrdOlbScheduler;
public:
XrdOlbJob    *NextJob;   // -> Next job in the queue (zero if last)
const char   *Comment;

virtual int   DoIt() = 0;

              XrdOlbJob(const char *desc="")
                    {Comment = desc; NextJob = 0; SchedTime = 0;}
virtual      ~XrdOlbJob() {}

private:
time_t        SchedTime; // -> Time job is to be scheduled
};

/******************************************************************************/
/*                  C l a s s   o o l b _ S c h e d u l e r                   */
/******************************************************************************/
  
class XrdOlbScheduler
{
public:

void          Schedule(XrdOlbJob *jp);

void          Schedule(XrdOlbJob *jp, time_t atime);

void          setWorkers(int minw, int maxw);

void          TimeSched();

void         *WorkIt();

              XrdOlbScheduler();

             ~XrdOlbScheduler();

private:

int          min_Workers; // Min threads we need in reserve
int          max_Workers; // Max threads we can start
int          num_Workers; // Number of threads we have

XrdOlbJob               *WorkQueue;  // Pending work to be serviced
XrdOlbJob               *WorkQLast;
XrdOucSemaphore          WorkAvail;
XrdOucMutex              WorkMutex;  // Protects private area

XrdOlbJob               *TimerQueue; // Pending work
XrdOucCondVar            TimerRings;
XrdOucMutex              TimerMutex; // Protects scheduler area

void hireWorker();
};
#endif
