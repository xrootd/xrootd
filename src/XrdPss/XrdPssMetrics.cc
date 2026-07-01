/******************************************************************************/
/*                                                                            */
/*                     X r d P s s M e t r i c s . c c                        */
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

#include "XrdPss/XrdPssMetrics.hh"
#include "XrdPosix/XrdPosixStats.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"

// The process-wide proxy I/O accounting, owned and updated by the XrdPosix layer
// on every open/close (XrdPosixXrootd.cc / XrdPosixFile.cc). The fields are plain
// 64-bit counters; a lockless read at scrape time is benign and matches the
// observed-metric idiom (the struct stays the source of truth).
//
namespace XrdPosixGlobals {extern XrdPosixStats Stats;}

void XrdPssRegisterMetrics(XrdMetrics::Collector &collector)
{
   XrdMetrics::Subsystem &subsystem = collector.subsystem("proxy");

   subsystem.observeCounter<std::uint64_t>("opens_total", {}, {}, "proxy file opens")
    .add({}, []{return (std::uint64_t)XrdPosixGlobals::Stats.X.Opens;});
   subsystem.observeCounter<std::uint64_t>("open_errors_total", {}, {}, "proxy file open errors")
    .add({}, []{return (std::uint64_t)XrdPosixGlobals::Stats.X.OpenErrs;});
   subsystem.observeCounter<std::uint64_t>("closes_total", {}, {}, "proxy file closes")
    .add({}, []{return (std::uint64_t)XrdPosixGlobals::Stats.X.Closes;});
   subsystem.observeCounter<std::uint64_t>("close_errors_total", {}, {}, "proxy file close errors")
    .add({}, []{return (std::uint64_t)XrdPosixGlobals::Stats.X.CloseErrs;});
}
