#ifndef __XRD_STATS_H__
#define __XRD_STATS_H__
/******************************************************************************/
/*                                                                            */
/*                           X r d S t a t s . h h                            */
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

#include <cstdlib>
#include <vector>

#include "XrdSys/XrdSysPthread.hh"

#define XRD_STATS_ADON   0x00000200
#define XRD_STATS_ALLJ   0x00000300
#define XRD_STATS_ALLX   0x000003FF
#define XRD_STATS_INFO   0x00000001
#define XRD_STATS_BUFF   0x00000002
#define XRD_STATS_LINK   0x00000004
#define XRD_STATS_PLUG   0x00000100
#define XRD_STATS_POLL   0x00000008
#define XRD_STATS_PROC   0x00000010
#define XRD_STATS_PROT   0x00000020
#define XRD_STATS_SCHD   0x00000040
#define XRD_STATS_SGEN   0x00000080
#define XRD_STATS_SYNC   0x40000000
#define XRD_STATS_SYNCA  0x20000000

#define XRD_STATS_JSON   0x10000000

class XrdOucEnv;
class XrdNetMsg;
class XrdMonitor;
class XrdScheduler;
class XrdBuffManager;

struct iovec;

class XrdStats
{
public:

void  Export(XrdOucEnv& env);

void  Init(char **Dest, int iVal=600, int xOpts=0, int jOpts=0);

void  Report();

class CallBack
     {public: virtual void Info(const  char*  data,  int dlen) = 0;
              virtual void Info(struct iovec* ioVec, int iovn) = 0;
                           CallBack() {}
              virtual     ~CallBack() {}
     };

// The following method is virtual only because we need to defer ld's
// symbol resolution to avoid reporting a missing symbol in libServer.so.
// This class is linked into the executable 'xrootd" so that XrdConfig
// class can resolve the symbol that will be passed on to plugins.
// This is a packaging issue that needs to get resolved at some point.
//
virtual
void  Stats(XrdStats::CallBack *InfoBack, int xOpts, int jOpts=0);

      XrdStats(XrdSysError *eP, XrdScheduler *sP, XrdBuffManager *bP,
               const char *hn, int port, const char *in, const char *pn,
               const char *sn);

virtual ~XrdStats() {if (buff) free(buff);}

private:

const char *GenStats(int &rsz, int opts);
void        GenStats(std::vector<struct iovec>& ioVec, int opts);
int         InfoStats(char *buff, int blen, int dosync=0);
int         ProcStats(char *buff, int blen, int dosync=0);

static long     tBoot;       // Time at boot time

XrdNetMsg      *netDest[2] = {0,0};
XrdScheduler   *XrdSched;
XrdSysError    *XrdLog;
XrdBuffManager *BuffPool;
XrdMonitor     *theMon;
XrdSysMutex     statsMutex;

int         blen;
char       *buff;        // Used by all callers
char       *Head;
const char *Hend;
char       *Jead;
int         Htln;
int         Jtln;
const char *Jend;

int         jsonOpts;
int         xmlOpts;
bool        autoSync = false;

const char *myHost;
const char *myName;
int         myPort;
};
#endif
