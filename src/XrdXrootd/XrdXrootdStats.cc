/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d S t a t s . c c                      */
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
 
#include <stdio.h>
  
#include "Xrd/XrdStats.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdXrootd/XrdXrootdResponse.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
 
/******************************************************************************/
/*                           C o n s t r c u t o r                            */
/******************************************************************************/
  
XrdXrootdStats::XrdXrootdStats(XrdStats *sp)
{

xstats   = sp;
fsP      = 0;

Count    = 0;     // Stats: Number of matches
errorCnt = 0;     // Stats: Number of errors returned
redirCnt = 0;     // Stats: Number of redirects
stallCnt = 0;     // Stats: Number of stalls
getfCnt  = 0;     // Stats: Number of getfiles
putfCnt  = 0;     // Stats: Number of putfiles
openCnt  = 0;     // Stats: Number of opens
readCnt  = 0;     // Stats: Number of reads
prerCnt  = 0;     // Stats: Number of reads
rvecCnt  = 0;     // Stats: Number of readv
rsegCnt  = 0;     // Stats: Number of readv segments
writeCnt = 0;     // Stats: Number of writes
syncCnt  = 0;     // Stats: Number of sync
miscCnt  = 0;     // Stats: Number of miscellaneous
AsyncNum = 0;     // Stats: Number of async ops
AsyncMax = 0;     // Stats: Number of async max
AsyncRej = 0;     // Stats: Number of async rejected
AsyncNow = 0;     // Stats: Number of async now (not locked)
Refresh  = 0;     // Stats: Number of refresh requests
LoginAT  = 0;     // Stats: Number of   attempted     logins
LoginAU  = 0;     // Stats: Number of   authenticated logins
LoginUA  = 0;     // Stats: Number of unauthenticated logins
AuthBad  = 0;     // Stats: Number of authentication failures
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdXrootdStats::Stats(char *buff, int blen, int do_sync)
{
   static const char statfmt[] = "<stats id=\"xrootd\"><num>%d</num>"
   "<ops><open>%d</open><rf>%d</rf><rd>%lld</rd><pr>%lld</pr>"
   "<rv>%lld</rv><rs>%lld</rs><wr>%lld</wr>"
   "<sync>%d</sync><getf>%d</getf><putf>%d</putf><misc>%d</misc></ops>"
   "<aio><num>%lld</num><max>%d</max><rej>%lld</rej></aio>"
   "<err>%d</err><rdr>%lld</rdr><dly>%d</dly>"
   "<lgn><num>%d</num><af>%d</af><au>%d</au><ua>%d</ua></lgn></stats>";
//                                   1 2 3 4 5 6 7 8
   static const long long LLMax = 0x7fffffffffffffffLL;
   static const int       INMax = 0x7fffffff;
   int len;

// If no buffer, caller wants the maximum size we will generate
//
   if (!buff)
      {char dummy[4096]; // Almost any size will do
       len = snprintf(dummy, sizeof(dummy), statfmt, INMax, INMax, INMax, LLMax,
                      LLMax, LLMax, LLMax, LLMax, INMax, INMax,
                      INMax, INMax,
                      LLMax, INMax, LLMax, INMax, LLMax, INMax,
                      INMax, INMax, INMax, INMax);
       return len + (fsP ? fsP->getStats(0,0) : 0);
      }

// Format our statistics
//
   statsMutex.Lock();
   len = snprintf(buff, blen, statfmt, Count, openCnt, Refresh, readCnt,
                  prerCnt, rvecCnt, rsegCnt, writeCnt, syncCnt, getfCnt,
                  putfCnt, miscCnt,
                  AsyncNum, AsyncMax, AsyncRej, errorCnt, redirCnt, stallCnt,
                  LoginAT, AuthBad, LoginAU, LoginUA);
   statsMutex.UnLock();

// Now include filesystem statistics and return
//
   if (fsP) len += fsP->getStats(buff+len, blen-len);
   return len;
}
 
/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdXrootdStats::Stats(XrdXrootdResponse &resp, const char *opts)
{
    class statsInfo : public XrdStats::CallBack
         {public:  void Info(const char *buff, int bsz)
                            {rc = respP->Send((void *)buff, bsz+1);}
                        statsInfo(XrdXrootdResponse *rP) : respP(rP), rc(0) {}
                       ~statsInfo() {}
          XrdXrootdResponse *respP;
          int rc;
         };
    statsInfo statsResp(&resp);
    int xopts = 0;

    while(*opts)
         {switch(*opts)
                {case 'a': xopts |= XRD_STATS_ALL;  break;
                 case 'b': xopts |= XRD_STATS_BUFF; break;    // b_uff
                 case 'i': xopts |= XRD_STATS_INFO; break;    // i_nfo
                 case 'l': xopts |= XRD_STATS_LINK; break;    // l_ink
                 case 'd': xopts |= XRD_STATS_POLL; break;    // d_evice
                 case 'u': xopts |= XRD_STATS_PROC; break;    // u_sage
                 case 'p': xopts |= XRD_STATS_PROT; break;    // p_rotocol
                 case 's': xopts |= XRD_STATS_SCHD; break;    // s_scheduler
                 default:  break;
                }
          opts++;
         }

    if (!xopts) return resp.Send();

    xstats->Stats(&statsResp, xopts);
    return statsResp.rc;
}
