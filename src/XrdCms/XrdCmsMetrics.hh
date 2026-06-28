#ifndef __XRDCMS_METRICS_HH__
#define __XRDCMS_METRICS_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d C m s M e t r i c s . h h                        */
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

namespace XrdMetrics {class Registry; Registry& Default();}

namespace XrdCms
{
//-----------------------------------------------------------------------------
//! Register the cmsd clustering metrics (group "cms") into the given registry.
//! Drives the per-subsystem registration on the Cluster, CmsState and RRQ
//! singletons; called once from XrdCmsConfig after the role labels are frozen.
//! Cluster-aggregate only (no per-node series), so cardinality is independent
//! of cluster size. Safe to call for any role; on a pure server the cluster
//! tables are empty so the series simply read zero.
//-----------------------------------------------------------------------------
void RegisterMetrics(XrdMetrics::Registry &reg = XrdMetrics::Default());
}
#endif
