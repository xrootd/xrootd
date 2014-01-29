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

#include <stdlib.h>

#include "XrdSys/XrdSysPthread.hh"

#define XRD_STATS_ALL    0x000000FF
#define XRD_STATS_INFO   0x00000001
#define XRD_STATS_BUFF   0x00000002
#define XRD_STATS_LINK   0x00000004
#define XRD_STATS_POLL   0x00000008
#define XRD_STATS_PROC   0x00000010
#define XRD_STATS_PROT   0x00000020
#define XRD_STATS_SCHD   0x00000040
#define XRD_STATS_SGEN   0x00000080
#define XRD_STATS_SYNC   0x40000000
#define XRD_STATS_SYNCA  0x20000000

class XrdScheduler;
class XrdBuffManager;

class XrdStats
{
public:

void  Report(char **Dest=0, int iVal=600, int Opts=0);

class CallBack
     {public: virtual void Info(const char *data, int dlen) = 0;
                           CallBack() {}
              virtual     ~CallBack() {}
     };

virtual
void  Stats(CallBack *InfoBack, int opts);

      XrdStats(XrdSysError *eP, XrdScheduler *sP, XrdBuffManager *bP,
               const char *hn, int port, const char *in, const char *pn,
               const char *sn);

virtual ~XrdStats() {if (buff) free(buff);}

private:

const char *GenStats(int &rsz, int opts);
int        InfoStats(char *buff, int blen, int dosync=0);
int        ProcStats(char *buff, int blen, int dosync=0);

static long     tBoot;       // Time at boot time

XrdScheduler   *XrdSched;
XrdSysError    *XrdLog;
XrdBuffManager *BuffPool;
XrdSysMutex     statsMutex;

char       *buff;        // Used by all callers
int         blen;
int         Hlen;
char       *Head;
const char *myHost;
const char *myName;
int         myPort;
};
#endif
