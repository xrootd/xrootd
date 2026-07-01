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

// Build every instrument once and return references to them. Labelled families
// (bytes_total, files_total, disk_bytes, meta_bytes) are created once here; the
// per-series handles are pulled with .labels(). The order matches the Handles
// struct's member order so the aggregate initialiser binds each reference.
//
CacheMetrics::Handles CacheMetrics::registerHandles(XrdMetrics::Collector &collector)
{
   XrdMetrics::Subsystem &subsystem = collector.subsystem("cache");

   auto &bytes = subsystem.counterFamily<std::uint64_t>("bytes_total", "bytes served by the cache by data origin", {"origin"});
   auto &files = subsystem.counterFamily<std::uint64_t>("files_total", "cache file lifecycle events", {"event"});
   auto &disk  = subsystem.gaugeFamily<std::int64_t>("disk_bytes", "cache data filesystem space", {"state"});
   auto &meta  = subsystem.gaugeFamily<std::int64_t>("meta_bytes", "cache metadata filesystem space", {"state"});

   return Handles
   {
      bytes.labels({"hit"}),      // from cache disk
      bytes.labels({"miss"}),     // fetched and cached
      bytes.labels({"bypass"}),   // direct, not cached
      subsystem.counter<std::uint64_t>("bytes_written_total", "bytes written to the cache disk"),
      subsystem.counter<std::uint64_t>("checksum_errors_total", "checksum errors fetching from origin"),
      files.labels({"opened"}),
      files.labels({"closed"}),
      files.labels({"created"}),
      files.labels({"removed"}),
      disk.labels({"total"}),
      disk.labels({"used"}),
      subsystem.gauge<std::int64_t>("data_used_bytes", "bytes occupied by cached data files"),
      meta.labels({"total"}),
      meta.labels({"used"})
   };
}

CacheMetrics::CacheMetrics(XrdMetrics::Collector &collector)
   : m(registerHandles(collector)) {}

void CacheMetrics::AddFileStats(const Stats &d)
{
   if (d.m_BytesHit)      m.bytes_hit     += (std::uint64_t)d.m_BytesHit;
   if (d.m_BytesMissed)   m.bytes_miss    += (std::uint64_t)d.m_BytesMissed;
   if (d.m_BytesBypassed) m.bytes_bypass  += (std::uint64_t)d.m_BytesBypassed;
   if (d.m_BytesWritten)  m.bytes_written += (std::uint64_t)d.m_BytesWritten;
   if (d.m_NCksumErrors)  m.cksum_errors  += (std::uint64_t)d.m_NCksumErrors;
}

void CacheMetrics::FileOpened(bool existing_file)
{
   ++m.files_opened;
   if (!existing_file) ++m.files_created;
}

void CacheMetrics::FileClosed() {++m.files_closed;}

void CacheMetrics::FilesRemoved(int n)
{
   if (n > 0) m.files_removed += (std::uint64_t)n;
}

void CacheMetrics::SetSpace(long long disk_total, long long disk_used,
                            long long data_used, long long meta_total,
                            long long meta_used)
{
   m.disk_total = (std::int64_t)disk_total;
   m.disk_used  = (std::int64_t)disk_used;
   m.data_used  = (std::int64_t)data_used;
   m.meta_total = (std::int64_t)meta_total;
   m.meta_used  = (std::int64_t)meta_used;
}
