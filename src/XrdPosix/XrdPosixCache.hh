#ifndef __XRDOUPOSIXCACHE_HH__
#define __XRDOUPOSIXCACHE_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x C a c h e . h h                       */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdOucCacheStats;

class XrdPosixCache
{
public:

//-----------------------------------------------------------------------------
//! Convert a logical path to the path of the corresonding entry in the cache.
//!
//! @param  url     -> url of the directory or file to be converted.
//! @param  buff    -> buffer to receive the result.
//! @param  blen    The length of the buffer (should be at least 1024).
//!
//! @return =0      Buffer holds the result.
//! @return <0      Conversion failed, the return value is -errno.
//-----------------------------------------------------------------------------

int           CachePath(const char *url, char *buff, int blen);

//-----------------------------------------------------------------------------
//! Check cache status of a file.
//!
//! @param  url     -> url of the logical file to be checked in the cache.
//! @param  hold    When true, the file purge time is extended to allow the
//!                 file to be accessed before eligible for purging. When
//!                 false (the default) only status information is returned.
//!
//! @return >0      The file is fully cached.
//! @return =0      The file exists in the cache but is not fully cached.
//! @return <0      The file does not exist in the cache.
//-----------------------------------------------------------------------------

int           CacheQuery(const char *url, bool hold=false);

//-----------------------------------------------------------------------------
//! Remove directory from the cache.
//!
//! @param  path    -> filepath of directory to be removed
//!
//! @return 0       This method is currently not supported.
//-----------------------------------------------------------------------------

int           Rmdir(const char* path);

//-----------------------------------------------------------------------------
//! Rename a file or directory in the cache.
//!
//! @param  oldpath -> filepath of existing directory or file.
//! @param  newpath -> filepath the directory or file is to have.
//!
//! @return 0       This method is currently not supported.
//-----------------------------------------------------------------------------

int           Rename(const char* oldPath, const char* newPath);

//-----------------------------------------------------------------------------
//! Rename a file or directory in the cache.
//!
//! @param  path    -> filepath of existing directory or file. This is the
//!                    actual path in the cache (see CachePath()).
//! @param  sbuff   Reference to the stat structure to hold the information.
//!
//! @return =0      The sbuff hold the information.
//! @return !0      The file or direcory does not exist in the cache.
//-----------------------------------------------------------------------------

int           Stat(const char *path, struct stat &sbuff);

//-----------------------------------------------------------------------------
//! Rename a file or directory in the cache.
//!
//! @param  Stat    Reference to the statistics object to be filled in.
//-----------------------------------------------------------------------------

void          Statistics(XrdOucCacheStats &Stats);

//-----------------------------------------------------------------------------
//! Truncate a file in the cache.
//!
//! @param  path    -> filepath of file to be truncated.
//! @param  size    The size in bytes the file should have.
//!
//! @return 0       This method is currently not supported.
//-----------------------------------------------------------------------------

int           Truncate(const char* path, off_t size);

//-----------------------------------------------------------------------------
//! Remove a file from the cache.
//!
//! @param  path    -> filepath of file to be removed.
//!
//! @return =0      File was removed.
//! @return !0      File could not be removed, because of one of the below:
//!                 -EBUSY   - the file is in use.
//!                 -EAGAIN  - file is currently subject to internal processing.
//!                 -errno   - file was not removed, filesystem unlink() failed.
//-----------------------------------------------------------------------------

int           Unlink(const char* path);

//-----------------------------------------------------------------------------
//! Constructor and destructor.
//-----------------------------------------------------------------------------

              XrdPosixCache() {}
             ~XrdPosixCache() {}
};
#endif
