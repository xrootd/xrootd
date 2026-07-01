#ifndef __XRDOFS_STATS_H__
#define __XRDOFS_STATS_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O f s S t a t s . h h                         */
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

#include "XrdMetrics/XrdMetricsInstrument.hh"

class XrdOfsStats
{
public:

//! References to the native metric instruments (owned by the process-wide
//! XrdMetrics registry) that the OFS code updates directly. The counters are
//! atomic, so there is no lock on the update path and no scrape-time race
//! against a reader; open-file/handle counts that go up and down are gauges.
struct      StatsData
{
XrdMetrics::Gauge<std::int64_t> &numOpenR;    // Read
XrdMetrics::Gauge<std::int64_t> &numOpenW;    // Write
XrdMetrics::Gauge<std::int64_t> &numOpenP;    // Posc
XrdMetrics::Gauge<std::int64_t> &numHandles;
XrdMetrics::Counter<std::uint64_t>  &numUnpsist;  // Posc files not persisted
XrdMetrics::Counter<std::uint64_t>  &numRedirect;
XrdMetrics::Counter<std::uint64_t>  &numStarted;
XrdMetrics::Counter<std::uint64_t>  &numReplies;
XrdMetrics::Counter<std::uint64_t>  &numErrors;
XrdMetrics::Counter<std::uint64_t>  &numDelays;
XrdMetrics::Counter<std::uint64_t>  &numSeventOK;
XrdMetrics::Counter<std::uint64_t>  &numSeventER;
XrdMetrics::Counter<std::uint64_t>  &numTPCgrant;
XrdMetrics::Counter<std::uint64_t>  &numTPCdeny;
XrdMetrics::Counter<std::uint64_t>  &numTPCerrs;
XrdMetrics::Counter<std::uint64_t>  &numTPCexpr;
}           Data;

       int  Report(char *Buff, int Blen);

       void setRole(const char *theRole) {myRole = theRole;}

            XrdOfsStats() : Data(RegisterMetrics()), myRole("?") {}
           ~XrdOfsStats() {}

private:

//! Create the OFS metric families in the process-wide XrdMetrics registry and
//! return references to the owned instruments. Called once from the ctor.
static StatsData RegisterMetrics();

const char *myRole;
};
#endif
