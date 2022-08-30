#ifndef _XRDOSSAT_H
#define _XRDOSSAT_H
/******************************************************************************/
/*                                                                            */
/*                           X r d O s s A t . h h                            */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>
#include <sys/types.h>

struct stat;
class  XrdOss;
class  XrdOssDF;

/******************************************************************************/
//!
//! This class defines the object that handles extended operations that are
//! relative to an open directory. Create a single instance of this class by
//! passing it the pointer to the asociated file system (XrdOss) and use the
//! methods herein to effect various operations relative to an XrdOss directory.
//!
//! @note
//! 1) The following relative methods are not currently implemented:
//!    access() (aka faccessat), chmod() (aka fchmodat), chown() (aka fchownat),
//!    mkdir() (aka mkdirat), readlink() (aka readlinkat),
//!    rename (a.k.a renameat), symlink() (aka symlinkat), and
//!    utimes() (a.k.a utimesat).
//! 2) The path argument must be relative and is not subject to name2name()
//!    processing. This is in contrast to standard Unix "at" calls.
//! 3) Only the online copy of the target is subject to these calls. Use the
//!    the standard calls for remote storage backed file systems.
/******************************************************************************/
  
class XrdOssAt
{
public:

//-----------------------------------------------------------------------------
//! Open a directory relative to an open directory.
//!
//! @param  atDir  - Reference to the directory object to use.
//! @param  path   - Pointer to the relative path of the directory to be opened.
//! @param  env    - Reference to environmental information.
//! @param  ossDF  - Reference to where the directory object pointer is to be
//!                  returned upon success.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int  Opendir(XrdOssDF &atDir, const char *path, XrdOucEnv &env, XrdOssDF *&ossDF);

//-----------------------------------------------------------------------------
//! Open a file in r/o mode relative to an open directory.
//!
//! @param  atDir  - Reference to the directory object to use.
//! @param  path   - Pointer to the relative path of the file to be opened.
//! @param  env    - Reference to environmental information.
//! @param  ossDF  - Reference to where the file object pointer is to be
//!                  returned upon success.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int  OpenRO(XrdOssDF &atDir, const char *path, XrdOucEnv &env, XrdOssDF *&ossDF);

//-----------------------------------------------------------------------------
//! Remove a directory relative to an open directory. Only the online entry
//! is removed (use standard remdir() for tape backed systems).
//!
//! @param  atDir  - Reference to the directory object to use.
//! @param  path   - Pointer to the path of the directory to be removed.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int  Remdir(XrdOssDF &atDir, const char *path);

//-----------------------------------------------------------------------------
//! Return state information on the target relative to an open directory.
//!
//! @param  atDir  - Reference to the directory object to use.
//! @param  path   - Pointer to the path of the target to be interrogated.
//! @param  buf    - Reference to the structure where info it to be returned.
//! @param  opts   - Options:
//!                  At_dInfo - provide bdevID in st_rdev and partID in st_dev
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

static const int  At_dInfo = 0x00000001;

int  Stat(XrdOssDF &atDir, const char *path, struct stat &buf, int opts=0);

//-----------------------------------------------------------------------------
//! Remove a file relative to an open directory. Only the online copy is
//! is removed (use standard unlink() for tape backed systems).
//!
//! @param  atDir  - Reference to the directory object to use.
//! @param  path   - Pointer to the path of the file to be removed.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int  Unlink(XrdOssDF &atDir, const char *path);

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  ossfs  - Reference to the OSS system interface.
//-----------------------------------------------------------------------------

     XrdOssAt(XrdOss &ossfs) : ossFS(ossfs) {}

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

    ~XrdOssAt() {}

private:

XrdOss      &ossFS;
};
#endif
