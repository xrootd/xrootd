#ifndef __OLB_PREPARE__H
#define __OLB_PREPARE__H
/******************************************************************************/
/*                                                                            */
/*                      X r d O l b P r e p a r e . h h                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$
  
#include "XrdOlb/XrdOlbScheduler.hh"
#include "XrdOlb/XrdOlbServer.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucStream.hh"

/******************************************************************************/
/*                        X r d O l b P r e p A r g s                         */
/******************************************************************************/

class XrdOlbServer;

class XrdOlbPrepArgs : public XrdOlbJob
{
public:

char           *reqid;
char           *user;
char           *prty;
char           *mode;
char           *path;
struct iovec   *iovp;
int             iovn;

void            Clear() {reqid = user = prty = mode = path = 0;
                         iovp = 0; iovn = 0;}

int             DoIt() {if (!XrdOlbServer::Resume(this)) delete this;
                        return 0;
                       }

                XrdOlbPrepArgs() : XrdOlbJob("prepare") {Clear();}

XrdOlbPrepArgs &operator =(const XrdOlbPrepArgs &rhs)
                   {reqid = rhs.reqid;
                    user  = rhs.user;
                    prty  = rhs.prty;
                    mode  = rhs.mode;
                    path  = rhs.path;
                    iovp  = rhs.iovp;
                    iovn  = rhs.iovn;
                    return *this;
                   }
               ~XrdOlbPrepArgs()
                   {if (reqid)   free(reqid);
                    if (user)    free(user);
                    if (prty)    free(prty);
                    if (mode)    free(mode);
                    if (path)    free(path);
                    if (iovp)    free(iovp);
                   }
};
  
/******************************************************************************/
/*                   C l a s s   X r d O l b P r e p a r e                    */
/******************************************************************************/

class XrdOlbPrepare : public XrdOlbJob
{
public:

int        Add(XrdOlbPrepArgs &pargs);

int        Del(char *reqid);

int        Exists(char *path);

void       Gone(char *path);

int        DoIt() {Scrub();
                   if (prepif) 
                      SchedP->Schedule((XrdOlbJob *)this, scrubtime+time(0));
                   return 1;
                  }

int        Pending() {return NumFiles;}

int        Reset();

int        setParms(int rcnt, int stime, int deco=0);

int        setParms(char *ifpgm);

int        setParms(XrdOlbScheduler *sp) {SchedP = sp; return 0;}

           XrdOlbPrepare();
          ~XrdOlbPrepare() {}   // Never gets deleted

private:

void       Scrub();
int        startIF();

XrdOucMutex           PTMutex;
XrdOucHash<char>      PTable;
XrdOucStream          prepSched;
XrdOlbScheduler      *SchedP;
time_t                lastemsg;
pid_t                 preppid;
int                   NumFiles;
int                   doEcho;
int                   resetcnt;
int                   scrub2rst;
int                   scrubtime;
char                 *prepif;
};
#endif
