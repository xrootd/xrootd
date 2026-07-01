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

// Native, atomic instruments owned by the process-wide registry. Each family is
// created exactly once (the factories do not deduplicate by name); multiple
// series are pulled from the labelled families via withLabelValues. The metric
// names, labels and help are unchanged from the earlier observed version, so the
// serialized output and the legacy <stats id="ofs"> block are identical.
//
XrdOfsStats::StatsData XrdOfsStats::RegisterMetrics()
{
   XrdMetrics::Subsystem& g = XrdMetrics::Default().subsystem("ofs");

   auto& filesOpen = g.intGauge("files_open", {"mode"}, {}, "currently open files");
   auto& events    = g.counter("events_total", {"result"}, {}, "scheduled event outcomes");
   auto& tpc       = g.counter("tpc_total", {"result"}, {}, "third-party-copy outcomes");

   return StatsData
   {
      filesOpen.withLabelValues({"read"}),
      filesOpen.withLabelValues({"write"}),
      filesOpen.withLabelValues({"posc"}),
      g.intGauge("handles", {}, {}, "active file handles").noLabels(),
      g.counter("unpersisted_total", {}, {}, "posc files not persisted").noLabels(),
      g.counter("redirects_total",   {}, {}, "redirects issued").noLabels(),
      g.counter("started_total",     {}, {}, "background ops started").noLabels(),
      g.counter("replies_total",     {}, {}, "direct data replies").noLabels(),
      g.counter("errors_total",      {}, {}, "errors returned").noLabels(),
      g.counter("delays_total",      {}, {}, "delays returned").noLabels(),
      events.withLabelValues({"ok"}),
      events.withLabelValues({"error"}),
      tpc.withLabelValues({"granted"}),
      tpc.withLabelValues({"denied"}),
      tpc.withLabelValues({"error"}),
      tpc.withLabelValues({"expired"})
   };
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
