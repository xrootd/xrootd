#ifndef __XRDXROOTDJOB_HH_
#define __XRDXROOTDJOB_HH_
/******************************************************************************/
/*                                                                            */
/*                       X r d X r o o t d J o b . h h                        */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <sys/types.h>
  
#include "Xrd/XrdJob.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucTable.hh"

class XrdOucProg;
class XrdLink;
class XrdScheduler;
class XrdXrootdJob2Do;
class XrdXrootdResponse;

// Definition of options that can be passed to Schedule()
//
#define JOB_Sync   0x0001
#define JOB_Unique 0x0002

class XrdXrootdJob : public XrdJob
{
friend class XrdXrootdJob2Do;
public:

int      Cancel(const char *jkey=0, XrdXrootdResponse *resp=0);

void     DoIt();

// List() returns a list of all jobs in xml format
//
XrdOucTList *List(void);

// args[0]   if not null if prefixes the response
// args[1-n] are passed to the prgram
// The return value is whatever resp->Send() returns
//
int      Schedule(const char         *jkey,   // Job Identifier
                  const char        **args,   // Zero terminated arglist
                  XrdXrootdResponse  *resp,   // Response object
                  int                 Opts=0);// Options (see above)

         XrdXrootdJob(XrdScheduler *schp,       // -> Scheduler
                      XrdOucProg   *pgm,        // -> Program Object
                      const char   *jname,      // -> Job name
                      int           maxjobs=4); // Maximum simultaneous jobs
        ~XrdXrootdJob();

private:
void CleanUp(XrdXrootdJob2Do *jp);
int  sendResult(XrdXrootdResponse *resp,
                const char        *rpfx,
                XrdXrootdJob2Do   *job);

static const int              reScan = 15*60;

XrdSysMutex                   myMutex;
XrdScheduler                 *Sched;
XrdOucTable<XrdXrootdJob2Do>  JobTable;
XrdOucProg                   *theProg;
char                         *JobName;
int                           maxJobs;
int                           numJobs;
};
#endif
