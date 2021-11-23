/******************************************************************************/
/* XrdFfsPosix.hh C wrapper to some of the Xrootd Posix library functions     */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/* Author: Wei Yang (SLAC National Accelerator Laboratory, 2009)              */
/*         Contract DE-AC02-76-SFO0515 with the Department of Energy          */
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

#include <dirent.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/statvfs.h>


#ifdef __cplusplus
  extern "C" {
#endif

int            XrdFfsPosix_stat(const char *file_name, struct stat *buf);

DIR           *XrdFfsPosix_opendir(const char *dirname);
struct dirent *XrdFfsPosix_readdir(DIR *dirp);
int            XrdFfsPosix_closedir(DIR *dir);
int            XrdFfsPosix_mkdir(const char *path, mode_t mode);
int            XrdFfsPosix_rmdir(const char *path);

int            XrdFfsPosix_open(const char *pathname, int flags, mode_t mode);
off_t          XrdFfsPosix_lseek(int fildes, off_t offset, int whence);
ssize_t        XrdFfsPosix_read(int fd, void *buf, size_t count);
ssize_t        XrdFfsPosix_pread(int fildes, void *buf, size_t nbyte, off_t offset);
int            XrdFfsPosix_close(int fd);
ssize_t        XrdFfsPosix_write(int fildes, const void *buf, size_t nbyte);
ssize_t        XrdFfsPosix_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
int            XrdFfsPosix_fsync(int fildes);
int            XrdFfsPosix_unlink(const char *path);
int            XrdFfsPosix_rename(const char *oldpath, const char *newpath);
int            XrdFfsPosix_ftruncate(int fildes, off_t offset);
int            XrdFfsPosix_truncate(const char *path, off_t size);
long long      XrdFfsPosix_getxattr(const char *path, const char *name, void *value, unsigned long long size);

/* 
   XrdFfsPosix_clear_from_rdr_cache() can be used to clear a non-existing file/directory from redirector cache
   Note that this function is doesn't do the work in a atomical step.
*/ 
void           XrdFfsPosix_clear_from_rdr_cache(const char *rdrurl);

int            XrdFfsPosix_unlinkall(const char *rdrurl, const char *path, uid_t user_uid);
int            XrdFfsPosix_rmdirall(const char *rdrurl, const char *path, uid_t user_uid);
int            XrdFfsPosix_renameall(const char *rdrurl, const char *from, const char *to, uid_t user_uid);
int            XrdFfsPosix_truncateall(const char *rdrurl, const char *path, off_t size, uid_t user_uid);
int            XrdFfsPosix_readdirall(const char *rdrurl, const char *path, char ***direntarray, uid_t user_uid);
int            XrdFfsPosix_statvfsall(const char *rdrurl, const char *path, struct statvfs *stbuf, uid_t user_uid);
int            XrdFfsPosix_statall(const char *rdrurl, const char *path, struct stat *stbuf, uid_t user_uid);

#ifdef __cplusplus
  }
#endif
