#ifndef __XRDOFSTPCPROG_HH__
#define __XRDOFSTPCPROG_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C P r o g . h h                       */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysPthread.hh"
  
class XrdOfsTPCJob;
class XrdOucProg;
  
class XrdOfsTPCProg
{
public:

       void      Cancel() {JobStream.Drain();}

static int       Init(char *XfrProg, int Num);

       void      Run();

static
XrdOfsTPCProg   *Start(XrdOfsTPCJob *jP, int &rc);

       int       Xeq();

                 XrdOfsTPCProg(XrdOfsTPCProg *Prev, int num);

                ~XrdOfsTPCProg() {}
private:

static XrdSysMutex    pgmMutex;
static XrdOfsTPCProg *pgmIdle;
static const char    *XfrProg;

       XrdOucProg     Prog;
       XrdOucStream   JobStream;
       XrdOfsTPCProg *Next;
       XrdOfsTPCJob  *Job;
       char           eRec[1024];
       int            Pnum;
};
#endif
