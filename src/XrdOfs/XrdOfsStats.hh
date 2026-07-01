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
XrdMetrics::IntGauge &numOpenR;    // Read
XrdMetrics::IntGauge &numOpenW;    // Write
XrdMetrics::IntGauge &numOpenP;    // Posc
XrdMetrics::IntGauge &numHandles;
XrdMetrics::Counter  &numUnpsist;  // Posc files not persisted
XrdMetrics::Counter  &numRedirect;
XrdMetrics::Counter  &numStarted;
XrdMetrics::Counter  &numReplies;
XrdMetrics::Counter  &numErrors;
XrdMetrics::Counter  &numDelays;
XrdMetrics::Counter  &numSeventOK;
XrdMetrics::Counter  &numSeventER;
XrdMetrics::Counter  &numTPCgrant;
XrdMetrics::Counter  &numTPCdeny;
XrdMetrics::Counter  &numTPCerrs;
XrdMetrics::Counter  &numTPCexpr;
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
