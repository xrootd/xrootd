#ifndef __XRDPOSIXOSDEP_H__
#define __XRDPOSIXOSDEP_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x O s D e p . h h                       */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/
  
// Solaris does not have a statfs64 structure. So all interfaces use statfs.
//
#ifdef __solaris__
#define statfs64 statfs
#endif

// We need to avoid using dirent64 for MacOS platforms. We would normally
// include XrdSysPlatform.hh for this but this include file needs to be
// standalone. So, we replicate the dirent64 redefinition here, Additionally,
// off64_t, normally defined in Solaris and Linux, is cast as long long (the
// appropriate type for the next 25 years). The Posix interface only supports
// 64-bit offsets.
//
#if  defined(__APPLE__)
#if !defined(dirent64)
#define dirent64 dirent
#endif
#if !defined(off64_t)
#define off64_t long long
#endif

#if defined(__DARWIN_VERS_1050) && !__DARWIN_VERS_1050
#if !defined(stat64)
#define stat64 stat
#endif
#if !defined(statfs64)
#define statfs64 statfs
#endif
#endif

#if !defined(statvfs64)
#define statvfs64 statvfs
#endif
#define ELIBACC ESHLIBVERS
#endif

#ifdef __FreeBSD__
#define	dirent64 dirent
#define	ELIBACC EFTYPE
#endif

#if defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
#define ELIBACC EFTYPE
#endif

#endif
