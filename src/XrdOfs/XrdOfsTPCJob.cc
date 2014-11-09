/******************************************************************************/
/*                                                                            */
/*                       X r d O f s T P C J o b . c c                        */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include "XrdOfs/XrdOfsStats.hh"
#include "XrdOfs/XrdOfsTPCJob.hh"
#include "XrdOfs/XrdOfsTPCProg.hh"
#include "XrdOuc/XrdOucCallBack.hh"
#include "XrdSfs/XrdSfsInterface.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdSysError  OfsEroute;
extern XrdOfsStats  OfsStats;

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/

XrdSysMutex        XrdOfsTPCJob::jobMutex;
XrdOfsTPCJob      *XrdOfsTPCJob::jobQ     = 0;
XrdOfsTPCJob      *XrdOfsTPCJob::jobLast  = 0;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOfsTPCJob::XrdOfsTPCJob(const char *Url, const char *Org,
                           const char *Lfn, const char *Pfn,
                           const char *Cks, short lfnLoc[2])
                          : XrdOfsTPC(Url, Org, Lfn, Pfn, Cks), myProg(0),
                            Status(isWaiting)
{  lfnPos[0] = lfnLoc[0]; lfnPos[1] = lfnLoc[1]; }
  
/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
void XrdOfsTPCJob::Del()
{
   XrdOfsTPCJob *pP = 0;
   bool tpcCan = false;

// Remove from queue if we are still in the queue
//
   jobMutex.Lock();
   if (inQ)
      {if (this == jobQ) jobQ = Next;
          else {pP = jobQ;
                while(pP && pP->Next != this) pP = pP->Next;
                if (pP) pP->Next = Next;
               }
       if (this == jobLast) jobLast = pP;
       inQ = 0; tpcCan = true;
      } else if (Status == isRunning && myProg)
                {myProg->Cancel(); tpcCan = true;}

   if (tpcCan && Info.cbP)
      Info.Reply(SFS_ERROR, ECANCELED, "destination file prematurely closed");

// Delete the element if possible
//
   if (Refs <= 1) delete this;
      else Refs--;
   jobMutex.UnLock();
}

/******************************************************************************/
/*                                  D o n e                                   */
/******************************************************************************/
  
XrdOfsTPCJob *XrdOfsTPCJob::Done(XrdOfsTPCProg *pgmP, const char *eTxt, int rc)
{
   XrdSysMutexHelper jobMon(&jobMutex);
   XrdOfsTPCJob *jP;

// Indicate job status
//
   eCode = rc; Status = isDone;
   if (Info.Key) free(Info.Key);
   Info.Key = (rc ? strdup(eTxt) : 0);

// Check if we need to do a callback
//
   if (Info.cbP)
      {if (rc) Info.Reply(SFS_ERROR, rc, eTxt);
          else Info.Reply(SFS_OK, 0, "");
      }

// Check if anyone is waiting for a program
//
   if ((jP = jobQ))
      {if (jP == jobLast) jobQ = jobLast = 0;
          else            jobQ = jP->Next;
       jP->myProg = pgmP; jP->Refs++; jP->inQ = 0; jP->Status = isRunning;
       if (jP->Info.cbP) jP->Info.Reply(SFS_OK, 0, "");
      }

// Free up this job and return the next job, if any
//
   myProg = 0;
   if (Refs <= 1) delete this;
      else Refs--;
   return jP;
}

/******************************************************************************/
/*                                  S y n c                                   */
/******************************************************************************/

int XrdOfsTPCJob::Sync(XrdOucErrInfo *eRR)
{
   static const int cbWaitTime = 1800;
   XrdSysMutexHelper jobMon(&jobMutex);
   int               rc;

// If we are running then simply wait for the copy to complete
//
   if (Status == isRunning)
      {if (Info.SetCB(eRR)) return SFS_ERROR;
       eRR->setErrCode(cbWaitTime);
       return SFS_STARTED;
      }

// If we are done then return what we have (this can't change)
//
   if (Status == isDone)
      {if (eCode) {eRR->setErrInfo(eCode, Info.Key); return SFS_ERROR;}
       return SFS_OK;
      }

// The only thing left is that we are an unstarted job, so try to start it.
//
   if (inQ) {myProg = 0; rc = 0;}
      else if ((myProg = XrdOfsTPCProg::Start(this, rc)))
              {Refs++; Status = isRunning; return SFS_OK;}

// We could not allocate a program to this job. Check if this is due to an err
//
   if (rc)
      {OfsEroute.Emsg("TPC", rc, "create tpc job thread");
       Status = isDone;
       eCode  = ECANCELED;
       if (Info.Key) free(Info.Key);
       Info.Key = strdup("Copy failed; resources unavailable.");
       return Info.Fail(eRR, "resources unavailable", ECANCELED);
      }

// No programs available, place this job in callback mode
//
   if (Info.SetCB(eRR)) return SFS_ERROR;
   if (jobLast) {jobLast->Next = this; jobLast = this;}
      else jobQ = jobLast = this;
   inQ = 1; eRR->setErrCode(cbWaitTime);
   return SFS_STARTED;
}
