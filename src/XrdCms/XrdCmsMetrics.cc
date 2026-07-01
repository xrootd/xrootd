/******************************************************************************/
/*                                                                            */
/*                     X r d C m s M e t r i c s . c c                        */
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
/******************************************************************************/

#include <cstdint>

#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsMetrics.hh"
#include "XrdCms/XrdCmsRRQ.hh"
#include "XrdCms/XrdCmsState.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"

using namespace XrdCms;

/******************************************************************************/
/*               X r d C m s : : R e g i s t e r M e t r i c s               */
/******************************************************************************/

// Coordinator: wire the cmsd singletons into the "cms" group of the registry.
// The clustering counters/gauges below all read live state that drives routing
// and the state machine, so the registry only observes them (the existing
// variables remain the source of truth).
//
void XrdCms::RegisterMetrics(XrdMetrics::Collector &reg)
{
   Cluster.RegisterMetrics(reg);
   CmsState.RegisterMetrics(reg);
   RRQ.RegisterMetrics(reg);
}

/******************************************************************************/
/*            X r d C m s C l u s t e r : : R e g i s t e r M e t r i c s    */
/******************************************************************************/

void XrdCmsCluster::RegisterMetrics(XrdMetrics::Collector &reg)
{
   XrdMetrics::Subsystem &g = reg.subsystem("cms");

// Active data nodes currently subscribed to this manager/supervisor.
//
   g.observeIntGauge("nodes", {}, {}, "active data nodes in the cluster")
    .add({}, [this]{return (std::int64_t)NodeCnt;});

// Total selection attempts (monotonic).
//
   g.observeCounter("selections_total", {}, {},
                    "cluster node selection attempts")
    .add({}, [this]{return (std::uint64_t)SelTcnt.load();});

// Successful selections by access mode. These are recomputed periodically by
// MonRefs() as the live sum of per-node references, so they are a running
// snapshot (gauge), not a strictly monotonic counter.
//
   auto &sel = g.observeIntGauge("selections", {"mode"}, {},
                    "successful node selections by access mode (running total)");
   sel.add({"read"},  [this]{return (std::int64_t)SelRtot.load();});
   sel.add({"write"}, [this]{return (std::int64_t)SelWtot.load();});

// Aggregate cluster space (managers/supervisors only; zero elsewhere). Space()
// walks the node table under a read lock, which is fine at scrape cadence.
// DiskTotal is reported in GB and DiskFree in MB, so convert both to bytes.
//
   auto &spc = g.observeGauge("space_bytes", {"state"}, {},
                    "aggregate cluster disk space");
   spc.add({"total"}, [this]{XrdCms::SpaceData s; Space(s, ~SMask_t(0));
                             return (double)s.Total * (1024.0*1024.0*1024.0);});
   spc.add({"free"},  [this]{XrdCms::SpaceData s; Space(s, ~SMask_t(0));
                             return (double)s.TotFr * (1024.0*1024.0);});
}

/******************************************************************************/
/*              X r d C m s S t a t e : : R e g i s t e r M e t r i c s      */
/******************************************************************************/

void XrdCmsState::RegisterMetrics(XrdMetrics::Collector &reg)
{
   XrdMetrics::Subsystem &g = reg.subsystem("cms");

   g.observeIntGauge("nodes_active", {}, {}, "active subscribers")
    .add({}, [this]{return (std::int64_t)numActive;});
   g.observeIntGauge("nodes_staging", {}, {}, "subscribers that can stage")
    .add({}, [this]{return (std::int64_t)numStaging;});
   g.observeIntGauge("nodes_min", {}, {}, "minimum subscribers needed")
    .add({}, [this]{return (std::int64_t)minNodeCnt;});

// Suspend/stage are reported as 0/1 flags. Suspended is a bitmask of the
// suspend sources; expose whether any suspension is in effect.
//
   g.observeIntGauge("suspended", {}, {}, "1 if the node is suspended")
    .add({}, [this]{return (std::int64_t)(Suspended ? 1 : 0);});
   g.observeIntGauge("nostaging", {}, {}, "1 if staging is disabled")
    .add({}, [this]{return (std::int64_t)(NoStaging ? 1 : 0);});
}

/******************************************************************************/
/*                X r d C m s R R Q : : R e g i s t e r M e t r i c s        */
/******************************************************************************/

void XrdCmsRRQ::RegisterMetrics(XrdMetrics::Collector &reg)
{
   XrdMetrics::Subsystem &g = reg.subsystem("cms");

// Fast vs slow split for lookups and redirects served by the response queue.
// Stats is updated under myMutex; a lockless read of an aligned 64-bit field at
// scrape time is benign and matches the observed-gauge idiom used elsewhere.
//
   auto &lu = g.observeCounter("lookups_total", {"speed"}, {},
                    "file location lookups served by the response queue");
   lu.add({"fast"}, [this]{return (std::uint64_t)Stats.luFast;});
   lu.add({"slow"}, [this]{return (std::uint64_t)Stats.luSlow;});

   auto &rd = g.observeCounter("redirects_total", {"speed"}, {},
                    "client redirects served by the response queue");
   rd.add({"fast"}, [this]{return (std::uint64_t)Stats.rdFast;});
   rd.add({"slow"}, [this]{return (std::uint64_t)Stats.rdSlow;});

   g.observeCounter("queued_total", {}, {},
                    "requests added to the response queue")
    .add({}, [this]{return (std::uint64_t)Stats.Add2Q;});
   g.observeCounter("responses_total", {}, {},
                    "responses delivered to waiting requests")
    .add({}, [this]{return (std::uint64_t)Stats.Resp;});
}
