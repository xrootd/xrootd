#ifndef __XRD_STATS_LEGACY_HH__
#define __XRD_STATS_LEGACY_HH__
/******************************************************************************/
/*                                                                            */
/*                   X r d S t a t s L e g a c y . h h                        */
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

namespace XrdMetrics { class MetricSnapshot; }

//-----------------------------------------------------------------------------
//! Renders the legacy XrdStats "<stats id=...>" XML blocks from a snapshot of
//! the new XrdMetrics registry. Each block hard-codes the historical element
//! layout and pulls its values by native metric name, so the registry can be
//! the single source of truth while still reproducing the pre-existing report
//! format byte-for-byte. All knowledge of the legacy schema is confined here,
//! keeping the XrdMetrics core clean.
//!
//! The methods follow the same convention as the per-subsystem Stats() methods
//! they mirror: pass a null buffer to obtain an upper bound on the size, or a
//! real buffer to write into it; the byte count written is returned.
//-----------------------------------------------------------------------------

class XrdStatsLegacy
{
public:

//! <stats id="info"> — server identity; the values are strings/ports that are
//! not numeric metrics, so they are passed in rather than read from a snapshot.
static int Info(const char* host, int port, const char* name,
                char* buff, int blen);

//! <stats id="proc"> — process CPU usage read once from getrusage() directly,
//! since the legacy block reports seconds/microseconds split, not the registry's
//! cpu_seconds doubles.
static int Proc(char* buff, int blen);

//! <stats id="buff"> — xlStats is the nested buffer-XL block (produced by the
//! caller) spliced in verbatim, matching XrdBuffManager::Stats().
static int Buff(const XrdMetrics::MetricSnapshot& snap, const char* xlStats,
                char* buff, int blen);

static int Link(const XrdMetrics::MetricSnapshot& snap, char* buff, int blen);

static int Poll(const XrdMetrics::MetricSnapshot& snap, char* buff, int blen);

static int Sched(const XrdMetrics::MetricSnapshot& snap, char* buff, int blen);

//! <stats id="xrootd"> — the xrootd protocol block. With a null buffer returns
//! the maximum block size (every field at its type's max), matching
//! XrdXrootdStats::Stats(); the nested <stats id="ofs"> filesystem block is
//! appended separately by the caller.
static int Xrootd(const XrdMetrics::MetricSnapshot& snap, char* buff, int blen);
};
#endif
