#ifndef __XRDPFC_METRICS_HH__
#define __XRDPFC_METRICS_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d P f c M e t r i c s . h h                        */
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

namespace XrdMetrics
{
class Collector;
class Counter;
template <class T> class Gauge;
Collector& Default();
}

namespace XrdPfc
{
class Stats;

//-----------------------------------------------------------------------------
//! Owned cache metrics (group "cache"). The cache accounting is pure (not part
//! of any control flow), so the registry is the source of truth: counters are
//! incremented and gauges set at the single aggregation boundary in
//! ResourceMonitor (process_queues for the lifecycle/byte counters,
//! update_vs_and_file_usage_info for the space gauges). All updates run on the
//! resource-monitor thread, so no per-I/O hot path is touched.
//-----------------------------------------------------------------------------
class CacheMetrics
{
public:
explicit CacheMetrics(XrdMetrics::Collector &reg = XrdMetrics::Default());

//! Fold one per-file delta (bytes hit/missed/bypassed/written, checksum
//! errors) into the counters.
void AddFileStats(const Stats &delta);

//! File lifecycle events. FileOpened also bumps "created" for a new file.
void FileOpened(bool existing_file);
void FileClosed();
void FilesRemoved(int n);

//! Refresh the space gauges (all values in bytes).
void SetSpace(long long disk_total, long long disk_used, long long data_used,
              long long meta_total, long long meta_used);

private:
XrdMetrics::Counter *m_bytes_hit;
XrdMetrics::Counter *m_bytes_miss;
XrdMetrics::Counter *m_bytes_bypass;
XrdMetrics::Counter *m_bytes_written;
XrdMetrics::Counter *m_cksum_errors;

XrdMetrics::Counter *m_files_opened;
XrdMetrics::Counter *m_files_closed;
XrdMetrics::Counter *m_files_created;
XrdMetrics::Counter *m_files_removed;

XrdMetrics::Gauge<std::int64_t> *m_disk_total;
XrdMetrics::Gauge<std::int64_t> *m_disk_used;
XrdMetrics::Gauge<std::int64_t> *m_data_used;
XrdMetrics::Gauge<std::int64_t> *m_meta_total;
XrdMetrics::Gauge<std::int64_t> *m_meta_used;
};
}
#endif
