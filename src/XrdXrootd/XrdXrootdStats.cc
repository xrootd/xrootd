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
 
#include <cstdio>

#include "Xrd/XrdStats.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdXrootd/XrdXrootdFileStats.hh"
#include "XrdXrootd/XrdXrootdResponse.hh"
#include "XrdXrootd/XrdXrootdStats.hh"

/******************************************************************************/
/*           F i l e   I / O   b y t e   t o t a l s   ( g l o b a l )        */
/******************************************************************************/

// Process-wide file I/O byte counters, updated by XrdXrootdFileStats on every
// read/write regardless of the per-file monitoring level (see the header).
//
RAtomic_llong XrdXrootdFileStats::totRdBytes{0};
RAtomic_llong XrdXrootdFileStats::totRvBytes{0};
RAtomic_llong XrdXrootdFileStats::totWrBytes{0};
 
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
rsegCnt  = 0;     // Stats: Number of readv  segments
wvecCnt  = 0;     // Stats: Number of writev
wsegCnt  = 0;     // Stats: Number of writev segments
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
aokSCnt  = 0;     // Stats: Number of signature successes
badSCnt  = 0;     // Stats: Number of signature failures
ignSCnt  = 0;     // Stats: Number of signature ignored

RegisterMetrics();
}

/******************************************************************************/
/*                       R e g i s t e r M e t r i c s                        */
/******************************************************************************/

void XrdXrootdStats::RegisterMetrics()
{
// The xrootd protocol counters keep their existing flat names, so they live in
// the registry's empty subsystem group (prefix only). Each reader reads the
// live counter atomically so the value is consistent with the concurrent
// AtomicInc/AtomicAdd updates on the hot path; the counters themselves remain
// the source of truth and these only observe them.
//
   XrdMetrics::MetricGroup& g = XrdMetrics::Default().group("");

   auto& ops = g.observeCounter("ops_total", {"op"}, {},
                                "xrootd protocol operations");
#define OPS(label, fld) ops.add({label}, [this]{return (uint64_t)AtomicGet(fld);})
   OPS("open",    openCnt);
   OPS("read",    readCnt);
   OPS("preread", prerCnt);
   OPS("readv",   rvecCnt);
   OPS("write",   writeCnt);
   OPS("writev",  wvecCnt);
   OPS("sync",    syncCnt);
   OPS("getfile", getfCnt);
   OPS("putfile", putfCnt);
   OPS("refresh", Refresh);
   OPS("misc",    miscCnt);
#undef OPS

   auto& lgn = g.observeCounter("logins_total", {"result"}, {},
                                "xrootd login outcomes");
#define LGN(label, fld) lgn.add({label}, [this]{return (uint64_t)AtomicGet(fld);})
   LGN("attempt",  LoginAT);
   LGN("auth",     LoginAU);
   LGN("noauth",   LoginUA);
   LGN("authfail", AuthBad);
#undef LGN

   auto& sig = g.observeCounter("signatures_total", {"result"}, {},
                                "xrootd request signature checks");
#define SIG(label, fld) sig.add({label}, [this]{return (uint64_t)AtomicGet(fld);})
   SIG("ok",      aokSCnt);
   SIG("bad",     badSCnt);
   SIG("ignored", ignSCnt);
#undef SIG

#define CTR(name, help, fld) \
   g.observeCounter(name, {}, {}, help) \
    .add({}, [this]{return (uint64_t)AtomicGet(fld);})
   CTR("requests_total",        "xrootd protocol requests",      Count);
   CTR("readv_segments_total",  "readv segments read",           rsegCnt);
   CTR("writev_segments_total", "writev segments written",       wsegCnt);
   CTR("async_ops_total",       "asynchronous i/o operations",   AsyncNum);
   CTR("async_rejected_total",  "rejected asynchronous i/o ops", AsyncRej);
   CTR("errors_total",          "errors returned to clients",    errorCnt);
   CTR("redirects_total",       "client redirects issued",       redirCnt);
   CTR("stalls_total",          "client stalls (delays) issued", stallCnt);
#undef CTR

// File I/O byte totals (counted on every read/write across all files). pgread
// folds into read, pgwrite/writev into write.
//
   g.observeCounter("bytes_total", {"op"}, {}, "file I/O bytes")
    .add({"read"},  []{return (uint64_t)XrdXrootdFileStats::totRdBytes.load();})
    .add({"readv"}, []{return (uint64_t)XrdXrootdFileStats::totRvBytes.load();})
    .add({"write"}, []{return (uint64_t)XrdXrootdFileStats::totWrBytes.load();});

// High-water mark of concurrent async i/o operations is a gauge, not a counter.
//
   g.observeIntGauge("async_max", {}, {}, "peak concurrent asynchronous i/o ops")
    .add({}, [this]{return (int64_t)AtomicGet(AsyncMax);});
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdXrootdStats::Stats(char *buff, int blen, int do_sync)
{
   static const char statfmt[] = "<stats id=\"xrootd\"><num>%d</num>"
   "<ops><open>%d</open><rf>%d</rf><rd>%lld</rd><pr>%lld</pr>"
   "<rv>%lld</rv><rs>%lld</rs>"
   "<wv>%lld</wv><ws>%lld</ws><wr>%lld</wr>"
   "<sync>%d</sync><getf>%d</getf><putf>%d</putf><misc>%d</misc></ops>"
   "<sig><ok>%d</ok><bad>%d</bad><ign>%d</ign></sig>"
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
       len = snprintf(dummy, sizeof(dummy), statfmt,
                      INMax, INMax, INMax, LLMax,
                      LLMax, LLMax, LLMax, LLMax, LLMax, LLMax, INMax, INMax,
                      INMax, INMax,
                      INMax, INMax, INMax,
                      LLMax, INMax, LLMax, INMax, LLMax, INMax,
                      INMax, INMax, INMax, INMax);
       return len + (fsP ? fsP->getStats(0,0) : 0);
      }

// Format our statistics
//
   statsMutex.Lock();
   len = snprintf(buff, blen, statfmt,
                  Count,   openCnt, Refresh, readCnt,
                  prerCnt, rvecCnt, rsegCnt, wvecCnt, wsegCnt, writeCnt,
                  syncCnt, getfCnt,
                  putfCnt, miscCnt,
                  aokSCnt, badSCnt, ignSCnt,
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
                   void Info(struct iovec* ioVec, int iovn)
                            {rc = respP->Send(ioVec, iovn);}
                        statsInfo(XrdXrootdResponse *rP) : respP(rP), rc(0) {}
                       ~statsInfo() {}
          XrdXrootdResponse *respP;
          int rc;
         };
    statsInfo statsResp(&resp);
    int xopts = 0;

    while(*opts)
         {switch(*opts)
                {case 'a': xopts |= XRD_STATS_ALLX; break;
                 case 'b': xopts |= XRD_STATS_BUFF; break;    // b_uff
                 case 'd': xopts |= XRD_STATS_POLL; break;    // d_evice
                 case 'i': xopts |= XRD_STATS_INFO; break;    // i_nfo
// Not yet       case 'J': xopts |= XRD_STATS_JSON; break;    // Want JSON
                 case 'l': xopts |= XRD_STATS_LINK; break;    // l_ink
                 case 'n': xopts |= XRD_STATS_ADON; break;    // addo_n
                 case 'p': xopts |= XRD_STATS_PROT; break;    // p_rotocol
                 case 'P': xopts |= XRD_STATS_PLUG; break;    // P_lugins
                 case 's': xopts |= XRD_STATS_SCHD; break;    // s_scheduler
                 case 'u': xopts |= XRD_STATS_PROC; break;    // u_sage
                 default:  break;
                }
          opts++;
         }

    if (!xopts) return resp.Send();

    xstats->Stats(&statsResp, xopts, 0);
    return statsResp.rc;
}
