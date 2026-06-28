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

static int Sched(const XrdMetrics::MetricSnapshot& snap, char* buff, int blen);
};
#endif
