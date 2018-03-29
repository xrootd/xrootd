#ifndef __XROOTD_STATS_H__
#define __XROOTD_STATS_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d S t a t s . h h                      */
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

#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucStats.hh"

class XrdSfsFileSystem;
class XrdStats;
class XrdXrootdResponse;

class XrdXrootdStats : public XrdOucStats
{
public:
int              Count;        // Stats: Number of matches
int              errorCnt;     // Stats: Number of errors returned
long long        redirCnt;     // Stats: Number of redirects
int              stallCnt;     // Stats: Number of stalls
int              getfCnt;      // Stats: Number of getfiles
int              putfCnt;      // Stats: Number of putfiles
int              openCnt;      // Stats: Number of opens
long long        readCnt;      // Stats: Number of reads
long long        prerCnt;      // Stats: Number of reads (pre)
long long        rsegCnt;      // Stats: Number of readv  segments
long long        rvecCnt;      // Stats: Number of reads
long long        wsegCnt;      // Stats: Number of writev segments
long long        wvecCnt;      // Stats: Number of writev
long long        writeCnt;     // Stats: Number of writes
int              syncCnt;      // Stats: Number of sync
int              miscCnt;      // Stats: Number of miscellaneous
long long        AsyncNum;     // Stats: Number of async ops
long long        AsyncRej;     // Stats: Number of async rejected
long long        AsyncNow;     // Stats: Number of async now (not locked)
int              AsyncMax;     // Stats: Number of async max
int              Refresh;      // Stats: Number of refresh requests
int              LoginAT;      // Stats: Number of   attempted     logins
int              LoginAU;      // Stats: Number of   authenticated logins
int              LoginUA;      // Stats: Number of unauthenticated logins
int              AuthBad;      // Stats: Number of authentication failures
int              aokSCnt;      // Stats: Number of signature successes
int              badSCnt;      // Stats: Number of signature failures
int              ignSCnt;      // Stats: Number of signature ignored

void             setFS(XrdSfsFileSystem *fsp) {fsP = fsp;}

int              Stats(char *buff, int blen, int do_sync=0);

int              Stats(XrdXrootdResponse &resp, const char *opts);

                 XrdXrootdStats(XrdStats *sp);
                ~XrdXrootdStats() {}
private:

XrdSfsFileSystem *fsP;
XrdStats *xstats;
};
#endif
