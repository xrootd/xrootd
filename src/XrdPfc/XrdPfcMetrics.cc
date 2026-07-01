/******************************************************************************/
/*                                                                            */
/*                     X r d P f c M e t r i c s . c c                        */
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

#include "XrdPfc/XrdPfcMetrics.hh"
#include "XrdPfc/XrdPfcStats.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"

using namespace XrdPfc;

CacheMetrics::CacheMetrics(XrdMetrics::Collector &collector)
{
   XrdMetrics::Subsystem &subsystem = collector.subsystem("cache");

// Bytes served, split by where the data came from.
//
   auto &bytes = subsystem.counter<std::uint64_t>("bytes_total", "bytes served by the cache by data origin", {}, {"origin"});
   m_bytes_hit    = &bytes.withLabelValues({"hit"});      // from cache disk
   m_bytes_miss   = &bytes.withLabelValues({"miss"});     // fetched and cached
   m_bytes_bypass = &bytes.withLabelValues({"bypass"});   // direct, not cached

   m_bytes_written = &subsystem.counter<std::uint64_t>("bytes_written_total", "bytes written to the cache disk").noLabels();
   m_cksum_errors  = &subsystem.counter<std::uint64_t>("checksum_errors_total", "checksum errors fetching from origin").noLabels();

// File lifecycle events.
//
   auto &files = subsystem.counter<std::uint64_t>("files_total", "cache file lifecycle events", {}, {"event"});
   m_files_opened  = &files.withLabelValues({"opened"});
   m_files_closed  = &files.withLabelValues({"closed"});
   m_files_created = &files.withLabelValues({"created"});
   m_files_removed = &files.withLabelValues({"removed"});

// Disk and metadata space, in bytes.
//
   auto &disk = subsystem.gauge<std::int64_t>("disk_bytes", "cache data filesystem space", {}, {"state"});
   m_disk_total = &disk.withLabelValues({"total"});
   m_disk_used  = &disk.withLabelValues({"used"});

   m_data_used  = &subsystem.gauge<std::int64_t>("data_used_bytes", "bytes occupied by cached data files").noLabels();

   auto &meta = subsystem.gauge<std::int64_t>("meta_bytes", "cache metadata filesystem space", {}, {"state"});
   m_meta_total = &meta.withLabelValues({"total"});
   m_meta_used  = &meta.withLabelValues({"used"});
}

void CacheMetrics::AddFileStats(const Stats &d)
{
   if (d.m_BytesHit)      *m_bytes_hit     += (std::uint64_t)d.m_BytesHit;
   if (d.m_BytesMissed)   *m_bytes_miss    += (std::uint64_t)d.m_BytesMissed;
   if (d.m_BytesBypassed) *m_bytes_bypass  += (std::uint64_t)d.m_BytesBypassed;
   if (d.m_BytesWritten)  *m_bytes_written += (std::uint64_t)d.m_BytesWritten;
   if (d.m_NCksumErrors)  *m_cksum_errors  += (std::uint64_t)d.m_NCksumErrors;
}

void CacheMetrics::FileOpened(bool existing_file)
{
   ++*m_files_opened;
   if (!existing_file) ++*m_files_created;
}

void CacheMetrics::FileClosed() {++*m_files_closed;}

void CacheMetrics::FilesRemoved(int n)
{
   if (n > 0) *m_files_removed += (std::uint64_t)n;
}

void CacheMetrics::SetSpace(long long disk_total, long long disk_used,
                            long long data_used, long long meta_total,
                            long long meta_used)
{
   *m_disk_total = (std::int64_t)disk_total;
   *m_disk_used  = (std::int64_t)disk_used;
   *m_data_used  = (std::int64_t)data_used;
   *m_meta_total = (std::int64_t)meta_total;
   *m_meta_used  = (std::int64_t)meta_used;
}
