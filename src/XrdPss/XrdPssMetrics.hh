#ifndef __XRDPSS_METRICS_HH__
#define __XRDPSS_METRICS_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d P s s M e t r i c s . h h                        */
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

namespace XrdMetrics {class Collector; Collector& Default();}

//-----------------------------------------------------------------------------
//! Register the proxy storage metrics (group "proxy") into the given registry.
//! XrdPss keeps no counters of its own; the proxy open/close accounting lives in
//! the shared XrdPosix layer (XrdPosixGlobals::Stats), so these series observe
//! that already-centralized struct rather than duplicating its increments.
//! Called once from XrdPssSys::Configure, i.e. only when running as a proxy.
//-----------------------------------------------------------------------------
void XrdPssRegisterMetrics(XrdMetrics::Collector &reg = XrdMetrics::Default());
#endif
