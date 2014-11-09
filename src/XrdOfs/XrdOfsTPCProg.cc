/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C P r o g . c c                       */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdio.h>
#include <strings.h>
  
#include "XrdOfs/XrdOfsTPC.hh"
#include "XrdOfs/XrdOfsTPCJob.hh"
#include "XrdOfs/XrdOfsTPCProg.hh"
#include "XrdOfs/XrdOfsTrace.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucCallBack.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdSysError  OfsEroute;
extern XrdOucTrace  OfsTrace;
extern XrdOss      *XrdOfsOss;

namespace XrdOfsTPCParms
{
extern char        *XfrProg;
extern char        *cksType;
extern int          xfrMax;
extern int          errMon;
extern bool         doEcho;
extern bool         autoRM;
};

using namespace XrdOfsTPCParms;

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdSysMutex        XrdOfsTPCProg::pgmMutex;
XrdOfsTPCProg     *XrdOfsTPCProg::pgmIdle  = 0;

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/
  
void *XrdOfsTPCProgRun(void *pp)
{
     XrdOfsTPCProg *theProg = (XrdOfsTPCProg *)pp;
     theProg->Run();
     return (void *)0;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOfsTPCProg::XrdOfsTPCProg(XrdOfsTPCProg *Prev, int num, int errMon)
             : Prog(&OfsEroute, errMon),
               JobStream(&OfsEroute),
               Next(Prev), Job(0)
             {snprintf(Pname, sizeof(Pname), "TPC job %d: ", num);
              Pname[sizeof(Pname)-1] = 0;
             }

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdOfsTPCProg::Init()
{
   int n;

// Allocate copy program objects
//
   for (n = 0; n < xfrMax; n++)
       {pgmIdle = new XrdOfsTPCProg(pgmIdle, n, errMon);
        if (pgmIdle->Prog.Setup(XfrProg, &OfsEroute)) return 0;
       }

// All done
//
   doEcho = doEcho || GTRACE(debug);
   return 1;
}

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/

void XrdOfsTPCProg::Run()
{
   int rc;

// Run the current job and indicate it's ending status and possibly getting a
// another job to run. Note "Job" will always be valid.
//
do{rc = Xeq();
   Job = Job->Done(this, eRec, rc);
  } while(Job);

// No more jobs to run. Place us on the idle queue. Upon return this thread
// will end.
//
   pgmMutex.Lock();
   Next = pgmIdle;
   pgmIdle = this;
   pgmMutex.UnLock();
}
  
/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
XrdOfsTPCProg *XrdOfsTPCProg::Start(XrdOfsTPCJob *jP, int &rc)
{
   XrdSysMutexHelper pgmMon(&pgmMutex);
   XrdOfsTPCProg    *pgmP;
   pthread_t         tid;

// Get a new program object, if none left, tell the caller to try later
//
   if (!(pgmP = pgmIdle)) {rc = 0; return 0;}
   pgmP->Job = jP;

// Start a thread to run the job
//
   if ((rc = XrdSysThread::Run(&tid, XrdOfsTPCProgRun, (void *)pgmP, 0,
                                "TPC job")))
      return 0;

// We are all set, return the program being used
//
   pgmIdle = pgmP->Next;
   return pgmP;
}

/******************************************************************************/
/*                                   X e q                                    */
/******************************************************************************/
  
int XrdOfsTPCProg::Xeq()
{
   EPNAME("Xeq");
   const char *cksOpt;
   char *lP, *Colon, *cksVal, *tident = Job->Info.Org;
   int rc;

// Echo out what we are doing if so desired
//
   if (doEcho)
      {char *Quest = index(Job->Info.Key, '?');
       if (Quest) *Quest = 0;
       OfsEroute.Say(Pname,tident," copying ",Job->Info.Key," to ",Job->Info.Dst);
       if (Quest) *Quest = '?';
      }

// Determine checksum option
//
   cksVal = (Job->Info.Cks ? Job->Info.Cks : XrdOfsTPCParms::cksType);
   cksOpt = (cksVal ? "-C" : 0);

// Start the job.
//
   if ((rc = Prog.Run(&JobStream,cksOpt,cksVal,Job->Info.Key,Job->Info.Dst)))
      {strcpy(eRec, "Copy failed; unable to start job.");
       OfsEroute.Emsg("TPC", Job->Info.Org, Job->Info.Lfn, eRec);
       return rc;
      }

// Now we drain the output looking for an end of run line. This line should
// be printed as an error message should the copy fail.
//
   *eRec = 0;
   while((lP = JobStream.GetLine()))
        {if ((Colon = index(lP, ':')) && *(Colon+1) == ' ')
            {strncpy(eRec, Colon+2, sizeof(eRec)); eRec[sizeof(eRec)-1] = 0;}
         if (doEcho && *lP) OfsEroute.Say(Pname, lP);
        }

// The job has completed. So, we must get the ending status.
//
   if ((rc = Prog.RunDone(JobStream)) < 0) rc = -rc;
   DEBUG(Pname <<"ended with rc=" <<rc);

// Check if we should generate a message
//
   if (rc && !(*eRec)) sprintf(eRec, "Copy failed with return code %d", rc);

// Log failures and optionally remove the file
//
   if (rc)
      {OfsEroute.Emsg("TPC", Job->Info.Org, Job->Info.Lfn, eRec);
       if (autoRM) XrdOfsOss->Unlink(Job->Info.Dst, XRDOSS_isPFN);
      }

// All done
//
   return rc;
}
