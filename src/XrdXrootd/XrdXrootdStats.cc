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
#include "Xrd/XrdStatsLegacy.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
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
/*          P e r - o p e r a t i o n   a d m i n   c o u n t e r s           */
/******************************************************************************/

namespace
{
// Cached per-request-id counter handles for the metadata/admin operations that
// otherwise fold into miscCnt. Indexed by (reqid - kXR_auth); populated once in
// RegisterMetrics, then a pointer deref on the dispatch hot path.
//
XrdMetrics::Counter<std::uint64_t> *adminOpCtr[kXR_REQFENCE - kXR_auth] = {nullptr};
}
 
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
   XrdMetrics::Subsystem& subsystem = XrdMetrics::Default().subsystem("");

   auto& ops = subsystem.observeCounter<std::uint64_t>("ops_total", "xrootd protocol operations", {}, {"op"});
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

   auto& lgn = subsystem.observeCounter<std::uint64_t>("logins_total", "xrootd login outcomes", {}, {"result"});
#define LGN(label, fld) lgn.add({label}, [this]{return (uint64_t)AtomicGet(fld);})
   LGN("attempt",  LoginAT);
   LGN("auth",     LoginAU);
   LGN("noauth",   LoginUA);
   LGN("authfail", AuthBad);
#undef LGN

   auto& sig = subsystem.observeCounter<std::uint64_t>("signatures_total", "xrootd request signature checks", {}, {"result"});
#define SIG(label, fld) sig.add({label}, [this]{return (uint64_t)AtomicGet(fld);})
   SIG("ok",      aokSCnt);
   SIG("bad",     badSCnt);
   SIG("ignored", ignSCnt);
#undef SIG

#define CTR(name, help, fld) \
   subsystem.observeCounter<std::uint64_t>(name, help) \
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
   subsystem.observeCounter<std::uint64_t>("bytes_total", "file I/O bytes", {}, {"op"})
    .add({"read"},  []{return (uint64_t)XrdXrootdFileStats::totRdBytes.load();})
    .add({"readv"}, []{return (uint64_t)XrdXrootdFileStats::totRvBytes.load();})
    .add({"write"}, []{return (uint64_t)XrdXrootdFileStats::totWrBytes.load();});

// High-water mark of concurrent async i/o operations is a gauge, not a counter.
//
   subsystem.observeGauge<std::int64_t>("async_max", "peak concurrent asynchronous i/o ops")
    .add({}, [this]{return (int64_t)AtomicGet(AsyncMax);});

// Async i/o operations currently in flight. AsyncNow gates async scheduling on
// the hot path, so it stays the source of truth and is only observed here.
//
   subsystem.observeGauge<std::int64_t>("async_now", "asynchronous i/o operations in flight")
    .add({}, [this]{return (int64_t)AtomicGet(AsyncNow);});

// Per-operation counters for the metadata/admin requests that otherwise only
// add to miscCnt. These are native (the series is the source of truth, bumped in
// the dispatch path); one cached series handle per request id, labelled by the
// request name. The existing ops_total/requests_total are left untouched.
//
   auto& adm = subsystem.counter<std::uint64_t>("admin_ops_total", "metadata/admin protocol operations", {}, {"op"});
   for (int reqid : {kXR_chmod, kXR_dirlist, kXR_fattr,   kXR_locate,
                     kXR_mkdir, kXR_mv,      kXR_query,    kXR_prepare,
                     kXR_rm,    kXR_rmdir,   kXR_set,      kXR_stat,
                     kXR_statx, kXR_truncate})
       adminOpCtr[reqid - kXR_auth] =
           &adm.withLabelValues({XProtocol::reqName((kXR_unt16)reqid)});
}

/******************************************************************************/
/*                           B u m p A d m i n O p                            */
/******************************************************************************/

void XrdXrootdStats::BumpAdminOp(int reqid)
{
   unsigned int i = (unsigned int)(reqid - kXR_auth);
   if (i < (unsigned int)(kXR_REQFENCE - kXR_auth) && adminOpCtr[i])
      ++*adminOpCtr[i];
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdXrootdStats::Stats(char *buff, int blen, int do_sync)
{
   (void)do_sync;  // the registry readers are atomic; no separate sync needed
   int len;

// If no buffer, caller wants the maximum size we will generate. The xrootd block
// is now produced from the XrdMetrics registry by XrdStatsLegacy; the nested
// filesystem block is still appended here.
//
   if (!buff)
      {XrdMetrics::MetricSnapshot empty;
       len = XrdStatsLegacy::Xrootd(empty, 0, 0);
       return len + (fsP ? fsP->getStats(0,0) : 0);
      }

   XrdMetrics::MetricSnapshot snap;
   XrdMetrics::Default().serialize(snap);
   len = XrdStatsLegacy::Xrootd(snap, buff, blen);

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
