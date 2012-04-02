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
                           const char *Cks, short lfnLoc[2]);

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
