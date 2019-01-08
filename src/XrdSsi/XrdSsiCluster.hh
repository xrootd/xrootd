#ifndef __SSICLUSTER__
#define __SSICLUSTER__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i C l u s t e r . h h                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

//------------------------------------------------------------------------------
//! The XrdSsiCluster object provides methods to manage the names and resources
//! of a server node relative to the cluster in which it resides. A pointer to
//! object is passed to the XrdSsiServer object loaded as start-up. It remains
//! valid for the duration of the program.
//------------------------------------------------------------------------------

class XrdSsiCluster
{
public:

//------------------------------------------------------------------------------
//! Notify the cluster of a newly added endpoint name or whose state has 
//! changed on on this server node.
//!
//! @param  name  The logical name.
//! @param  pend  When true, the name is scheduled to be present in the future.
//------------------------------------------------------------------------------

virtual void   Added(const char *name, bool pend=false) = 0;

//------------------------------------------------------------------------------
//! Determine whether or not the SSI plug-in is running in a data context.
//!
//! @return true  running in a data context (i.e. xrootd).
//! @return false running is a meta context (i.e. cmsd).
//------------------------------------------------------------------------------

virtual bool          DataContext() = 0;

//------------------------------------------------------------------------------
//! Obtain the list of nodes that are managing this cluster.
//!
//! @param  mNum Place to put the number of managers in the returned array.
//!
//! @return The vector of nodes being used with mNum set to the number of
//!         elements. The list is considered permanent and is not deleted.
//------------------------------------------------------------------------------

virtual
const  char  * const *Managers(int &mNum) = 0;

//------------------------------------------------------------------------------
//! Notify the cluster that a name is no longer available on this server node.
//!
//! @param  name The logical name that is no longer available.
//------------------------------------------------------------------------------

virtual void   Removed(const char *name) = 0;

//------------------------------------------------------------------------------
//! Resume service after a suspension.
//!
//! @param  perm When true the resume persist across server restarts. Otherwise,
//!              it is treated as a temporary request.
//------------------------------------------------------------------------------

virtual void   Resume (bool perm=true) = 0;

//------------------------------------------------------------------------------
//! Suspend service.
//!
//! @param  perm When true the suspend persist across server restarts.
//!              Otherwise, it is treated as a temporary request.
//------------------------------------------------------------------------------

virtual void   Suspend(bool perm=true) = 0;

//------------------------------------------------------------------------------
//! Enable the Reserve() & Release() methods.
//!
//! @param  n  a positive integer that specifies the amount of resource units
//!            that are available. It may be reset at any time.
//!
//! @return The previous resource value. This first call returns 0.
//------------------------------------------------------------------------------

virtual int    Resource(int n) = 0;

//------------------------------------------------------------------------------
//! Decrease the amount of resources available. When the available resources
//! becomes non-positive, perform a temporary suspend to prevent additional
//! clients from being dispatched to this server.
//!
//! @param  n  The value by which resources are decreased (default 1).
//!
//! @return The amount of resource left.
//------------------------------------------------------------------------------

virtual int    Reserve (int n=1) = 0;

//------------------------------------------------------------------------------
//! Increase the amount of resource available. When transitioning from a
//! a non-positive to a positive resource amount, perform a resume so that
//! additional clients may be dispatched to this server.
//!
//! @param  n  The value to add to the resources available (default 1). The
//!            total amount is capped by the amount specified by Resource().
//!
//! @return The amount of resource left.
//------------------------------------------------------------------------------

virtual int    Release (int n=1) = 0;

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

               XrdSsiCluster() {}
virtual       ~XrdSsiCluster() {}

};
#endif
