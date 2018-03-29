#ifndef __CMS_PREPARE__H
#define __CMS_PREPARE__H
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s P r e p a r e . h h                       */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdCms/XrdCmsPrepArgs.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdFrcProxy;
class XrdNetMsg;
class XrdOucMsubs;
class XrdOucName2Name;

class XrdCmsPrepare : public XrdJob
{
public:

int        Add(XrdCmsPrepArgs &pargs);

int        Del(char *reqid);

int        Exists(char *path);

void       Gone(char *path);

void       DoIt();

void       Init();

void       Inform(const char *cmd, XrdCmsPrepArgs *pargs);

int        isOK() {return prepOK;}

int        Pending() {return NumFiles;}

void       Prepare(XrdCmsPrepArgs *pargs);

void       Reset(const char *iName, const char *aPath, int aMode);

int        setParms(int rcnt, int stime, int deco=0);

int        setParms(const char *ifpgm, char *ifmsg=0);

int        setParms(XrdOucName2Name *n2n) {N2N = n2n; return 0;}

           XrdCmsPrepare();
          ~XrdCmsPrepare() {}   // Never gets deleted

private:

int        isOnline(char *path);
void       Reset();
void       Scrub();
int        startIF();

XrdSysMutex           PTMutex;
XrdOucHash<char>      PTable;
XrdOucStream          prepSched;
XrdOucName2Name      *N2N;
XrdOucMsubs          *prepMsg;
XrdNetMsg            *Relay;
XrdFrcProxy          *PrepFrm;
char                 *prepif;
time_t                lastemsg;
pid_t                 preppid;
int                   prepOK;
int                   NumFiles;
int                   doEcho;
int                   resetcnt;
int                   scrub2rst;
int                   scrubtime;
};

namespace XrdCms
{
extern    XrdCmsPrepare PrepQ;
}
#endif
