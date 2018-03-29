#ifndef __SFS_FLAGS_H__
#define __SFS_FLAGS_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d S f s F l a g s . h h                         */
/*                                                                            */
/*(c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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

#include <sys/stat.h>
#include <fcntl.h>

//-----------------------------------------------------------------------------
//! This include file defines certain falgs that can be used by various Sfs
//! plug-ins to passthrough special attributes of regular files.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//! The following flags define the mode bit that can be used to mark a file
//! as close pending. This varies depending on the platform. This supports the
//! Persist On Successful Close (POSC) feature in an efficient way.
//-----------------------------------------------------------------------------

#ifdef __solaris__
#define XRDSFS_POSCPEND S_ISUID
#else
#define XRDSFS_POSCPEND S_ISVTX
#endif

//-----------------------------------------------------------------------------
//! The following bits may be set in the st_rdev member of the stat() structure
//! to indicate special attributes of a regular file. These bits are inspected
//! only when the remaining bits identified by XRD_RDVMASK are set to zero.
//! For backward compatability, offline status is also assumed when st_dev and
//! st_ino are both set to zero.
//-----------------------------------------------------------------------------

static const dev_t XRDSFS_OFFLINE =
                   static_cast<dev_t>(0x80LL<<((sizeof(dev_t)*8)-8));
static const dev_t XRDSFS_HASBKUP =
                   static_cast<dev_t>(0x40LL<<((sizeof(dev_t)*8)-8));
static const dev_t XRDSFS_RDVMASK =
                   static_cast<dev_t>(~(0xffLL<<((sizeof(dev_t)*8)-8)));
#endif
