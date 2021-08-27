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

#include <cstdint>
#include <sys/stat.h>
#include <fcntl.h>

//-----------------------------------------------------------------------------
//! This include file defines certain flags that can be used by various Sfs
//! plug-ins to passthrough features and special attributes of regular files.
//-----------------------------------------------------------------------------

namespace XrdSfs
{
//! Feature: Authorization
static const uint64_t hasAUTZ = 0x0000000000000001LL;

//! Feature: Checkpointing
static const uint64_t hasCHKP = 0x0000000000000002LL;

//! Feature: gpFile
static const uint64_t hasGPF  = 0x0000000000000004LL;

//! Feature: gpFile anonymous
static const uint64_t hasGPFA = 0x0000000000000008LL;

//! Feature: pgRead and pgWrite
static const uint64_t hasPGRW = 0x0000000000000010LL;

//! Feature: Persist On Successful Close
static const uint64_t hasPOSC = 0x0000000000000020LL;

//! Feature: Prepare Handler Version 2 (different calling conventions)
static const uint64_t hasPRP2 = 0x0000000000000040LL;

//! Feature: Proxy Server
static const uint64_t hasPRXY = 0x0000000000000080LL;

//! Feature: Supports SfsXio
static const uint64_t hasSXIO = 0x0000000000000100LL;

//! Feature: Supports no sendfile
static const uint64_t hasNOSF = 0x0000000000000200LL;

//! Feature: Implements a data cache
static const uint64_t hasCACH = 0x0000000000000400LL;

//! Feature: Supports no async I/O
static const uint64_t hasNAIO = 0x0000000000000800LL;
}

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
//! For backward compatibility, offline status is also assumed when st_dev and
//! st_ino are both set to zero.
//-----------------------------------------------------------------------------

static const dev_t XRDSFS_OFFLINE =
                   static_cast<dev_t>(0x80LL<<((sizeof(dev_t)*8)-8));
static const dev_t XRDSFS_HASBKUP =
                   static_cast<dev_t>(0x40LL<<((sizeof(dev_t)*8)-8));
static const dev_t XRDSFS_RDVMASK =
                   static_cast<dev_t>(~(0xffLL<<((sizeof(dev_t)*8)-8)));
#endif
