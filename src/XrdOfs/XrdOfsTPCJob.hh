#ifndef __XRDOFSTPCJOB_HH__
#define __XRDOFSTPCJOB_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d O f s T P C J o b . h h                        */
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
  
#include "XrdOfs/XrdOfsTPC.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdOfsTPCProg;

class XrdOfsTPCJob : public XrdOfsTPC
{
public:

void          Del();

XrdOfsTPCJob *Done(XrdOfsTPCProg *pgmP, const char *eTxt, int rc);

int           Sync(XrdOucErrInfo *eRR);

              XrdOfsTPCJob(const char *Url, const char *Org,
                           const char *Lfn, const char *Pfn,
                           const char *Cks, short lfnLoc[2],
                           const char *Spr, const char *Tpr);

             ~XrdOfsTPCJob() {}

private:
static XrdSysMutex        jobMutex;
static XrdOfsTPCJob      *jobQ;
static XrdOfsTPCJob      *jobLast;
       XrdOfsTPCJob      *Next;
       XrdOfsTPCProg     *myProg;
       int                eCode;
enum   jobStat {isWaiting, isRunning, isDone};
       jobStat            Status;
       short              lfnPos[2];
};
#endif
