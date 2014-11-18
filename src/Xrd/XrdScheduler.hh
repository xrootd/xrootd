#ifndef ___XRD_SCHED_H___
#define ___XRD_SCHED_H___
/******************************************************************************/
/*                                                                            */
/*                       X r d S c h e d u l e r . h h                        */
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

#include <unistd.h>
#include <sys/types.h>

#include "XrdSys/XrdSysPthread.hh"
#include "Xrd/XrdJob.hh"

class XrdOucTrace;
class XrdSchedulerPID;
class XrdSysError;

class XrdScheduler : public XrdJob
{
public:

int           Active() {return num_Workers - idl_Workers + num_JobsinQ;}

void          Cancel(XrdJob *jp);

inline int    canStick() {return  num_Workers              < stk_Workers
                              || (num_Workers-idl_Workers) < stk_Workers;}

void          DoIt();

pid_t         Fork(const char *id);

void         *Reaper();

void          Run();

void          Schedule(XrdJob *jp);
void          Schedule(int num, XrdJob *jfirst, XrdJob *jlast);
void          Schedule(XrdJob *jp, time_t atime);

void          setParms(int minw, int maxw, int avlt, int maxi, int once=0);

void          Start();

int           Stats(char *buff, int blen, int do_sync=0);

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
              XrdScheduler(XrdSysError *eP, XrdOucTrace *tP,
                           int minw=8, int maxw=8192, int maxi=780);

             ~XrdScheduler();

private:
XrdSysError *XrdLog;
XrdOucTrace *XrdTrace;

XrdSysMutex DispatchMutex; // Disp: Protects above area
int        idl_Workers;    // Disp: Number of idle workers

int        min_Workers;   // Sched: Min threads we need to have
int        max_Workers;   // Sched: Max threads we can start
int        max_Workidl;   // Sched: Max idle time for threads above min_Workers
int        num_Workers;   // Sched: Number of threads we have
int        stk_Workers;   // Sched: Number of sticky workers we can have
int        num_JobsinQ;   // Sched: Number of outstanding jobs in the queue
int        num_Layoffs;   // Sched: Number of threads to terminate

XrdJob                *WorkFirst;  // Pending work
XrdJob                *WorkLast;
XrdSysSemaphore        WorkAvail;
XrdSysMutex            SchedMutex; // Protects private area

XrdJob                *TimerQueue; // Pending work
XrdSysCondVar          TimerRings;
XrdSysMutex            TimerMutex; // Protects scheduler area

XrdSchedulerPID       *firstPID;
XrdSysMutex            ReaperMutex;

void hireWorker(int dotrace=1);
void Monitor();
void traceExit(pid_t pid, int status);
static const char *TraceID;
};
#endif
