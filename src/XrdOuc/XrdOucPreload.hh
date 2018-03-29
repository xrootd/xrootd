#ifndef __XRDOUCPRELOAD_HH__
#define __XRDOUCPRELOAD_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c P r e l o a d . h h                       */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
//! This include file define a utility function that pre-loads a plugin.
//------------------------------------------------------------------------------
  
//------------------------------------------------------------------------------
//! Preload a plugin and persist its image. Internal plugin version checking
//! is performed when the plugin is actually initialized.
//!
//! @param plib   Pointer to the shared library path that contains the plugin.
//! @param eBuff  Pointer to a buffer tat is to receive any messages. Upon
//!               failure it will contain an eror message. Upon success it
//!               will contain an informational message that describes the
//!               version that was loaded.
//! @param eBlen  The length of the eBuff, it should be at least 1K to
//!               avoid message truncation as the message may have a path.
//! @param retry  When true:  if the version name of the plugin is not found,
//!                           try to preload the unversioned name.
//!               When false: Only the versioned name of the plugin may be
//!                           preloaded (i.e. libXXXX-n.so).
//!
//! @return true  The plugin was successfully loaded.
//! @return false The plugin could not be loaded, eBuff contains the reason.
//------------------------------------------------------------------------------

extern bool XrdOucPreload(const char       *plib,
                                char       *eBuff,
                                int         eBlen,
                                bool        retry=false);
#endif
