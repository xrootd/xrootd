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
#include <cstring>
#include <sys/resource.h>
#include <sys/time.h>

#include "Xrd/XrdStatsLegacy.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

/******************************************************************************/
/*                                  I n f o                                   */
/******************************************************************************/

// Mirrors XrdStats::InfoStats(): pure server identity, no registry values.
//
int XrdStatsLegacy::Info(const char* host, int port, const char* name,
                         char* buff, int blen)
{
   static const char statfmt[] = "<stats id=\"info\"><host>%s</host>"
                     "<port>%d</port><name>%s</name></stats>";

   if (!buff) return sizeof(statfmt) + 24 + (host ? (int)strlen(host) : 0);

   return snprintf(buff, blen, statfmt, host, port, name);
}

/******************************************************************************/
/*                                  P r o c                                   */
/******************************************************************************/

// Mirrors XrdStats::ProcStats(): getrusage() is read once and its fields are
// formatted directly; the legacy block's seconds/microseconds split does not
// match the registry's cpu_seconds doubles, so the registry is not consulted.
//
int XrdStatsLegacy::Proc(char* buff, int blen)
{
   static const char statfmt[] = "<stats id=\"proc\">"
          "<usr><s>%lld</s><u>%lld</u></usr>"
          "<sys><s>%lld</s><u>%lld</u></sys>"
          "</stats>";

   if (!buff) return sizeof(statfmt) + 16*13;

   struct rusage r;
   if (getrusage(RUSAGE_SELF, &r)) return 0;

   return snprintf(buff, blen, statfmt,
          static_cast<long long>(r.ru_utime.tv_sec),
          static_cast<long long>(r.ru_utime.tv_usec),
          static_cast<long long>(r.ru_stime.tv_sec),
          static_cast<long long>(r.ru_stime.tv_usec));
}

/******************************************************************************/
/*                                  B u f f                                   */
/******************************************************************************/

// Mirrors XrdBuffManager::Stats(); xlStats is the nested buffer-XL block the
// caller renders and we splice in verbatim.
//
int XrdStatsLegacy::Buff(const XrdMetrics::MetricSnapshot& s,
                         const char* xlStats, char* buff, int blen)
{
   static const char statfmt[] = "<stats id=\"buff\"><reqs>%d</reqs>"
               "<mem>%lld</mem><buffs>%d</buffs><adj>%d</adj>%s</stats>";

   if (!buff) return sizeof(statfmt) + 16*4 + (xlStats ? (int)strlen(xlStats) : 0);

   return snprintf(buff, blen, statfmt,
       (int)      s.getInt("xrootd_buff_requests_total"),
       (long long)s.getInt("xrootd_buff_memory_bytes"),
       (int)      s.getInt("xrootd_buff_buffers"),
       (int)      s.getInt("xrootd_buff_adjustments_total"),
       xlStats ? xlStats : "");
}

/******************************************************************************/
/*                                  L i n k                                   */
/******************************************************************************/

// Mirrors XrdLinkXeq::Stats(); the in/out byte counters carry a dir label.
//
int XrdStatsLegacy::Link(const XrdMetrics::MetricSnapshot& s, char* buff, int blen)
{
   static const char statfmt[] = "<stats id=\"link\"><num>%d</num>"
          "<maxn>%d</maxn><tot>%lld</tot><in>%lld</in><out>%lld</out>"
          "<ctime>%lld</ctime><tmo>%d</tmo><stall>%d</stall>"
          "<sfps>%d</sfps></stats>";

   if (!buff) return sizeof(statfmt) + 17*6;

   return snprintf(buff, blen, statfmt,
       (int)      s.getInt("xrootd_link_connections"),
       (int)      s.getInt("xrootd_link_connections_max"),
       (long long)s.getInt("xrootd_link_connections_total"),
       (long long)s.getInt("xrootd_link_bytes_total{dir=\"in\"}"),
       (long long)s.getInt("xrootd_link_bytes_total{dir=\"out\"}"),
       (long long)s.getInt("xrootd_link_connect_seconds_total"),
       (int)      s.getInt("xrootd_link_timeouts_total"),
       (int)      s.getInt("xrootd_link_stalls_total"),
       (int)      s.getInt("xrootd_link_sendfile_interrupts_total"));
}

/******************************************************************************/
/*                                  P o l l                                   */
/******************************************************************************/

// Mirrors XrdPoll::Stats() (which sums the per-poller tallies the registry
// readers already aggregate).
//
int XrdStatsLegacy::Poll(const XrdMetrics::MetricSnapshot& s, char* buff, int blen)
{
   static const char statfmt[] = "<stats id=\"poll\"><att>%d</att>"
   "<en>%d</en><ev>%d</ev><int>%d</int></stats>";

   if (!buff) return sizeof(statfmt) + 16*4;

   return snprintf(buff, blen, statfmt,
       (int)s.getInt("xrootd_poll_attached"),
       (int)s.getInt("xrootd_poll_enabled"),
       (int)s.getInt("xrootd_poll_events_total"),
       (int)s.getInt("xrootd_poll_interrupts_total"));
}

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

/******************************************************************************/
/*                                X r o o t d                                 */
/******************************************************************************/

// Mirrors XrdXrootdStats::Stats() (the "<stats id=\"xrootd\">" portion). The
// operation, login and signature tallies come from the labelled ops_total /
// logins_total / signatures_total families; the rest from flat counters and the
// async_max gauge. The labelled lookups use the cached "name{labels}" key.
//
int XrdStatsLegacy::Xrootd(const XrdMetrics::MetricSnapshot& s, char* buff, int blen)
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

// With no buffer the caller wants the maximum size this block can reach, which
// (as in the original) is the format filled with each field's type maximum.
//
   if (!buff)
      {static const long long LLMax = 0x7fffffffffffffffLL;
       static const int       INMax = 0x7fffffff;
       char dummy[4096];
       return snprintf(dummy, sizeof(dummy), statfmt,
                       INMax, INMax, INMax, LLMax, LLMax, LLMax, LLMax,
                       LLMax, LLMax, LLMax, INMax, INMax, INMax, INMax,
                       INMax, INMax, INMax,
                       LLMax, INMax, LLMax, INMax, LLMax, INMax,
                       INMax, INMax, INMax, INMax);
      }

   return snprintf(buff, blen, statfmt,
       (int)      s.getInt("xrootd_requests_total"),
       (int)      s.getInt("xrootd_ops_total{op=\"open\"}"),
       (int)      s.getInt("xrootd_ops_total{op=\"refresh\"}"),
       (long long)s.getInt("xrootd_ops_total{op=\"read\"}"),
       (long long)s.getInt("xrootd_ops_total{op=\"preread\"}"),
       (long long)s.getInt("xrootd_ops_total{op=\"readv\"}"),
       (long long)s.getInt("xrootd_readv_segments_total"),
       (long long)s.getInt("xrootd_ops_total{op=\"writev\"}"),
       (long long)s.getInt("xrootd_writev_segments_total"),
       (long long)s.getInt("xrootd_ops_total{op=\"write\"}"),
       (int)      s.getInt("xrootd_ops_total{op=\"sync\"}"),
       (int)      s.getInt("xrootd_ops_total{op=\"getfile\"}"),
       (int)      s.getInt("xrootd_ops_total{op=\"putfile\"}"),
       (int)      s.getInt("xrootd_ops_total{op=\"misc\"}"),
       (int)      s.getInt("xrootd_signatures_total{result=\"ok\"}"),
       (int)      s.getInt("xrootd_signatures_total{result=\"bad\"}"),
       (int)      s.getInt("xrootd_signatures_total{result=\"ignored\"}"),
       (long long)s.getInt("xrootd_async_ops_total"),
       (int)      s.getInt("xrootd_async_max"),
       (long long)s.getInt("xrootd_async_rejected_total"),
       (int)      s.getInt("xrootd_errors_total"),
       (long long)s.getInt("xrootd_redirects_total"),
       (int)      s.getInt("xrootd_stalls_total"),
       (int)      s.getInt("xrootd_logins_total{result=\"attempt\"}"),
       (int)      s.getInt("xrootd_logins_total{result=\"authfail\"}"),
       (int)      s.getInt("xrootd_logins_total{result=\"auth\"}"),
       (int)      s.getInt("xrootd_logins_total{result=\"noauth\"}"));
}
