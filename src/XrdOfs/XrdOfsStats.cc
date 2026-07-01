/******************************************************************************/
/*                                                                            */
/*                        X r d O f s S t a t s . c c                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>
#include <cstdio>

#include "Xrd/XrdStatsLegacy.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdOfs/XrdOfsStats.hh"

/******************************************************************************/
/*                       R e g i s t e r M e t r i c s                        */
/******************************************************************************/

void XrdOfsStats::RegisterMetrics()
{
   XrdMetrics::Subsystem& g = XrdMetrics::Default().subsystem("ofs");

// Current open files (by mode) and active file handles go up and down, so they
// are gauges.
//
   g.observeIntGauge("files_open", {"mode"}, {}, "currently open files")
    .add({"read"},  [this]{return (std::int64_t)Data.numOpenR;})
    .add({"write"}, [this]{return (std::int64_t)Data.numOpenW;})
    .add({"posc"},  [this]{return (std::int64_t)Data.numOpenP;});
   g.observeIntGauge("handles", {}, {}, "active file handles")
    .add({}, [this]{return (std::int64_t)Data.numHandles;});

// The remaining tallies only ever increase, so they are counters.
//
#define CTR(name, help, fld) \
   g.observeCounter(name, {}, {}, help) \
    .add({}, [this]{return (std::uint64_t)(unsigned)Data.fld;})
   CTR("unpersisted_total", "posc files not persisted", numUnpsist);
   CTR("redirects_total",   "redirects issued",         numRedirect);
   CTR("started_total",     "background ops started",   numStarted);
   CTR("replies_total",     "direct data replies",      numReplies);
   CTR("errors_total",      "errors returned",          numErrors);
   CTR("delays_total",      "delays returned",          numDelays);
#undef CTR

   g.observeCounter("events_total", {"result"}, {}, "scheduled event outcomes")
    .add({"ok"},    [this]{return (std::uint64_t)(unsigned)Data.numSeventOK;})
    .add({"error"}, [this]{return (std::uint64_t)(unsigned)Data.numSeventER;});

   g.observeCounter("tpc_total", {"result"}, {}, "third-party-copy outcomes")
    .add({"granted"}, [this]{return (std::uint64_t)(unsigned)Data.numTPCgrant;})
    .add({"denied"},  [this]{return (std::uint64_t)(unsigned)Data.numTPCdeny;})
    .add({"error"},   [this]{return (std::uint64_t)(unsigned)Data.numTPCerrs;})
    .add({"expired"}, [this]{return (std::uint64_t)(unsigned)Data.numTPCexpr;});
}

/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/

// The <stats id="ofs"> block is now produced from the XrdMetrics registry by
// XrdStatsLegacy; the metrics registered above observe this instance's Data.
//
int XrdOfsStats::Report(char *buff, int blen)
{
   XrdMetrics::MetricSnapshot snap;
   if (buff) XrdMetrics::Default().serialize(snap);

   const int statsz = XrdStatsLegacy::Ofs(snap, myRole, nullptr, 0);

   if (!buff)            return statsz;
   if (blen  < statsz)   return 0;

   return XrdStatsLegacy::Ofs(snap, myRole, buff, blen);
}
