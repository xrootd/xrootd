/******************************************************************************/
/*                                                                            */
/*                   X r d S t a t s L e g a c y . c c                        */
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

#include <cstdio>

#include "Xrd/XrdStatsLegacy.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

/******************************************************************************/
/*                                 S c h e d                                  */
/******************************************************************************/

// Mirrors XrdScheduler::Stats(): the format string and field order are
// identical, but the values come from the registry snapshot by native name
// instead of from the scheduler's live members.
//
int XrdStatsLegacy::Sched(const XrdMetrics::MetricSnapshot& s, char* buff, int blen)
{
   static const char statfmt[] = "<stats id=\"sched\"><jobs>%d</jobs>"
               "<inq>%d</inq><maxinq>%d</maxinq>"
               "<threads>%d</threads><idle>%d</idle>"
               "<tcr>%d</tcr><tde>%d</tde>"
               "<tlimr>%d</tlimr></stats>";

   if (!buff) return sizeof(statfmt) + 16*8;

   return snprintf(buff, blen, statfmt,
       (int)s.getInt("xrootd_sched_jobs_total"),
       (int)s.getInt("xrootd_sched_jobs_in_queue"),
       (int)s.getInt("xrootd_sched_queue_length_max"),
       (int)s.getInt("xrootd_sched_threads"),
       (int)s.getInt("xrootd_sched_threads_idle"),
       (int)s.getInt("xrootd_sched_threads_created_total"),
       (int)s.getInt("xrootd_sched_threads_destroyed_total"),
       (int)s.getInt("xrootd_sched_thread_limit_hits_total"));
}
