#ifndef ___XRD_SCHED_H___
#define ___XRD_SCHED_H___
/******************************************************************************/
/*                                                                            */
/*                       X r d S c h e d u l e r . h h                        */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

#include <unistd.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucPthread.hh"
#include "Xrd/XrdJob.hh"

class XrdSchedulerPID;

class XrdScheduler : public XrdJob
{
public:

void          Cancel(XrdJob *jp);

inline int    canStick() {return num_Workers < stk_Workers
                              || idl_Workers > avl_Workers;}

void          DoIt();

pid_t         Fork(const char *id);

void         *Reaper();

void          Run();

void          Schedule(XrdJob *jp);
void          Schedule(int num, XrdJob *jfirst, XrdJob *jlast);
void          Schedule(XrdJob *jp, time_t atime);

void          setParms(int minw, int maxw, int avlt, int maxi);

void          Start(int mode);

int           Stats(char *buff, int blen);

void          TimeSched();

// Statistical information
//
int        num_TCreate; // Number of threads created
int        num_TDestroy;// Number of threads destroyed
int        num_Jobs;    // Number of jobs scheduled
int        max_QLength; // Longest queue length we had
int        num_Limited; // Number of times max was reached

// Constructor and destructor
//
              XrdScheduler(int minw=8, int maxw=16, int frt=4, int maxi=780);

             ~XrdScheduler();

private:
int        idl_Workers;   // Disp: Number of idle workers
XrdOucMutex DispatchMutex; // Disp: Protects above area

int        min_Workers;   // Sched: Min threads we need to have
int        max_Workers;   // Sched: Max threads we can start
int        max_Workidl;   // Sched: Max idle time for threads above min_Workers
int        num_Workers;   // Sched: Number of threads we have
int        avl_Workers;   // Sched: Number of readily available threads we need
int        stk_Workers;   // Sched: Number of sticky workers we can have
int        num_JobsinQ;   // Sched: Number of outstanding jobs in the queue

XrdJob                *WorkFirst;  // Pending work
XrdJob                *WorkLast;
XrdOucSemaphore        WorkAvail;
XrdOucMutex            SchedMutex; // Protects private area

XrdJob                *TimerQueue; // Pending work
XrdOucCondVar          TimerRings;
XrdOucMutex            TimerMutex; // Protects scheduler area

XrdSchedulerPID       *firstPID;
XrdOucMutex            ReaperMutex;

void hireWorker();
void Monitor();
void traceExit(pid_t pid, int status);
static const char *TraceID;
};
#endif
