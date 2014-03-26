#ifndef __XRDPOSIXXROOTD_H__
#define __XRDPOSIXXROOTD_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P o s i x X r o o t d                         */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif

#include "XrdPosix/XrdPosixOsDep.hh"
#include "XrdSys/XrdSysPthread.hh"

struct XrdOucIOVec;

class XrdScheduler;
class XrdOucCache;
class XrdOucEnv;
class XrdPosixCallBack;
class XrdPosixFile;

//-----------------------------------------------------------------------------
//! POSIX interface to XRootD with some extensions, as noted.
//-----------------------------------------------------------------------------

class XrdPosixXrootd
{
public:

//-----------------------------------------------------------------------------
//! Access() conforms to POSIX.1-2001 access()
//-----------------------------------------------------------------------------

static int     Access(const char *path, int amode);

//-----------------------------------------------------------------------------
//! Close() conforms to POSIX.1-2001 close()
//-----------------------------------------------------------------------------

static int     Close(int fildes);

//-----------------------------------------------------------------------------
//! Closedir() conforms to POSIX.1-2001 closedir()
//-----------------------------------------------------------------------------

static int     Closedir(DIR *dirp);

//-----------------------------------------------------------------------------
//! endPoint() is a POSIX extension and returns the location of an open file.
//!
//! @param  FD   File descriptor of an open file in question.
//! @param  Buff Pointer to the buffer to receive '<host>:<port>' of the server.
//! @param  Blen Size of the buffer, it must be big enough for the result.
//!
//! @return >= 0 The numeric port number of the data server.
//! @return -1   Call failed, errno has the reason.
//-----------------------------------------------------------------------------

static int     endPoint(int FD, char *Buff, int Blen);

//-----------------------------------------------------------------------------
//! Fstat() conforms to POSIX.1-2001 fstat()
//-----------------------------------------------------------------------------

static int     Fstat(int fildes, struct stat *buf);

//-----------------------------------------------------------------------------
//! Fsync() conforms to POSIX.1-2001 fsync()
//-----------------------------------------------------------------------------

static int     Fsync(int fildes);

//-----------------------------------------------------------------------------
//! Ftruncate() conforms to POSIX.1-2001 ftruncate()
//-----------------------------------------------------------------------------

static int     Ftruncate(int fildes, off_t offset);

//-----------------------------------------------------------------------------
//! Getxattr() is a POSIX extension and conforms to Linux 2.4 getxattr(). This
//! method returns attributes associated with a file. The format does not
//! correspond to information returned by Linux. Refer to the XRootD protocol
//! reference for the detailed description of the information returned.
//!
//! @param  path  pointer to the path whose attributes are to be returned
//! @param  name  name of the attribute to be returned. Valid attributes are
//!               xrootd.cksum  - file's checksum
//!               xrootd.space  - space associated with the path
//!               xrootd.xattr  - server specific extended attributes for path
//! @param  value pointer to the buffer to receive the attribute values.
//! @param  size  size of the buffer (value). If size is zero, only the
//!               maximum length of the attribute value is returned.
//! @return On success, a positive number is returned indicating the size of
//!         is extended attribute value. On failure, -1 is returned and errno
//!         is set to indicate the reason.
//!----------------------------------------------------------------------------

static long long Getxattr (const char *path, const char *name,
                           void *value, unsigned long long size);

//-----------------------------------------------------------------------------
//! Lseek() conforms to POSIX.1-2001 lseek()
//-----------------------------------------------------------------------------

static off_t   Lseek(int fildes, off_t offset, int whence);

//-----------------------------------------------------------------------------
//! Mkdir() conforms to POSIX.1-2001 mkdir()
//-----------------------------------------------------------------------------

static int     Mkdir(const char *path, mode_t mode);

//-----------------------------------------------------------------------------
//! Open() conforms to POSIX.1-2001 open() when extensions are not used.
//!
//! Extensions:
//! @param  cbP Pointer to a callback object. When specified, the open is
//!         performed in the background and the Comp[lete() is called when the
//!         Open() completes. See XrdPosixCallBack.hh for complete details.
//! @return -1 is always returned when cbP is specified. If the Open() was
//!         actually scheduled then errno will contain EINPROGRESS. Otherwise,
//!         the Open() immediately failed and errno contains the reason.
//-----------------------------------------------------------------------------

static const int isStream = 0x40000000; // Internal for Open oflag

static int     Open(const char *path, int oflag, mode_t mode=0,
                    XrdPosixCallBack *cbP=0);

//-----------------------------------------------------------------------------
//! Opendir() conforms to POSIX.1-2001 opendir()
//-----------------------------------------------------------------------------

static DIR*    Opendir(const char *path);

//-----------------------------------------------------------------------------
//! Pread() conforms to POSIX.1-2001 pread()
//-----------------------------------------------------------------------------
  
static ssize_t Pread(int fildes, void *buf, size_t nbyte, off_t offset);

//-----------------------------------------------------------------------------
//! Pwrite() conforms to POSIX.1-2001 pwrite()
//-----------------------------------------------------------------------------

static ssize_t Pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);

//-----------------------------------------------------------------------------
//! QueryChksum() is a POSIX extension and returns a file's modification time
//! and its associated checksum value.
//!
//! @param  path  path associated with the file whose checksum is wanted.
//! @param  mtime where the file's modification time (st_mtime) is placed.
//! @param  buff  pointer to the buffer to hold the checksum value.
//! @param  blen  the length of the buffer.
//!
//! @return Upon success returns the length of the checksum response placed in
//!         buff. Otherwise, -1 is returned and errno appropriately set.
//-----------------------------------------------------------------------------

static int     QueryChksum(const char *path, time_t &mtime,
                                 char *buff, int     blen);

//-----------------------------------------------------------------------------
//! QueryOpaque() is a POSIX extension and returns a file's implementation
//! specific information.
//!
//! @param  path  path associated with the file whose information is wanted.
//! @param  buff  pointer to the buffer to hold the information.
//! @param  blen  the length of the buffer.
//!
//! @return Upon success returns the length of the checksum response placed in
//!         buff. Otherwise, -1 is returned and errno appropriately set.
//-----------------------------------------------------------------------------

static long long QueryOpaque(const char *path, char *buff, int blen);

//-----------------------------------------------------------------------------
//! Read() conforms to POSIX.1-2001 read()
//-----------------------------------------------------------------------------
  
static ssize_t Read(int fildes, void *buf, size_t nbyte);

//-----------------------------------------------------------------------------
//! Readv() conforms to POSIX.1-2001 readv()
//-----------------------------------------------------------------------------

static ssize_t Readv(int fildes, const struct iovec *iov, int iovcnt);

//-----------------------------------------------------------------------------
//! readdir() conforms to POSIX.1-2001 readdir() and is normally equivalent
//! to readdir64(). The latter is provided for those platforms that require
//! a specific 64-bit interface to directory information, which is now rare.
//-----------------------------------------------------------------------------

static struct  dirent*   Readdir  (DIR *dirp);
static struct  dirent64* Readdir64(DIR *dirp);

//-----------------------------------------------------------------------------
//! readdir_r() conforms to POSIX.1-2001 readdir_r() and is normally equivalent
//! to readdir64_r(). The latter is provided for those platforms that require
//! a specific 64-bit interface to directory information, which is now rare.
//-----------------------------------------------------------------------------

static int     Readdir_r  (DIR *dirp, struct dirent   *entry, struct dirent   **result);
static int     Readdir64_r(DIR *dirp, struct dirent64 *entry, struct dirent64 **result);

//-----------------------------------------------------------------------------
//! Rename() conforms to POSIX.1-2001 rename()
//-----------------------------------------------------------------------------

static int     Rename(const char *oldpath, const char *newpath);

//-----------------------------------------------------------------------------
//! Rewinddir() conforms to POSIX.1-2001 rewinddir()
//-----------------------------------------------------------------------------

static void    Rewinddir(DIR *dirp);

//-----------------------------------------------------------------------------
//! Rmdir() conforms to POSIX.1-2001 rmdir()
//-----------------------------------------------------------------------------

static int     Rmdir(const char *path);

//-----------------------------------------------------------------------------
//! Seekdir() conforms to POSIX.1-2001 seekdir()
//-----------------------------------------------------------------------------

static void    Seekdir(DIR *dirp, long loc);

//-----------------------------------------------------------------------------
//! Stat() conforms to POSIX.1-2001 stat()
//-----------------------------------------------------------------------------

static int     Stat(const char *path, struct stat *buf);

//-----------------------------------------------------------------------------
//! Statfs() generally conforms to the platform-specific definition of statfs().
//! There is no specific POSIX specification for this call.
//-----------------------------------------------------------------------------

static int     Statfs(const char *path, struct statfs *buf);

//-----------------------------------------------------------------------------
//! Statvfs() conforms to POSIX.1-2001 statvfs()
//-----------------------------------------------------------------------------

static int     Statvfs(const char *path, struct statvfs *buf);

//-----------------------------------------------------------------------------
//! Telldir() conforms to POSIX.1-2001 telldir()
//-----------------------------------------------------------------------------

static long    Telldir(DIR *dirp);

//-----------------------------------------------------------------------------
//! Telldir() conforms to POSIX.1-2001 telldir()
//-----------------------------------------------------------------------------

static int     Truncate(const char *path, off_t offset);

//-----------------------------------------------------------------------------
//! Unlink() conforms to POSIX.1-2001 unlink()
//-----------------------------------------------------------------------------

static int     Unlink(const char *path);

//-----------------------------------------------------------------------------
//! VRead() is a POSIX extension and allows one to read multiple chunks of
//! a file in one operation.
//!
//! @param  fildes  file descriptor of a file opened for reading.
//! @param  readV   the read vector of offset/length/buffer triplets. Data at
//!                 each offset of the specifiued length is placed in buffer.
//! @param  n       the number of elements in the readV vector.
//!
//! @return Upon success returns the total number of bytes read. Otherwise, -1
//!         is returned and errno is appropriately set.
//-----------------------------------------------------------------------------

static ssize_t VRead(int fildes, const XrdOucIOVec *readV, int n);

//-----------------------------------------------------------------------------
//! Write() conforms to POSIX.1-2001 write()
//-----------------------------------------------------------------------------

static ssize_t Write(int fildes, const void *buf, size_t nbyte);

//-----------------------------------------------------------------------------
//! Writev() conforms to POSIX.1-2001 writev()
//-----------------------------------------------------------------------------

static ssize_t Writev(int fildes, const struct iovec *iov, int iovcnt);

//-----------------------------------------------------------------------------
//! The following methods are considered private but defined as public to
//! allow XrdPosix 'C' functions and XrdPss classes access private members.
//-----------------------------------------------------------------------------

inline int     fdOrigin() {return baseFD;}

static bool    isXrootdDir(DIR *dirp);

static bool    myFD(int fd);

static void    setCache(XrdOucCache *cP);

static void    setDebug(int val, bool doDebug=false);

static void    setEnv(const char *kword, int kval);

static void    setIPV4(bool userv4);

static void    setSched(XrdScheduler *sP);

/* There must be one instance of this object per executable image. Typically,
   this object is declared in main() or at file level. This is necessary to
   properly do one-time initialization of the static members. When declaring
   this object, you can pass the following information:
   maxfd  - maximum number of simultaneous files and directories to support.
            The value returned by getrlimit() over-rides the passed value
            unless maxfd is negative. When negative, abs(maxfd) becomes the
            absolute maximum and shadow file descriptors are not used.
   maxdir - Ignored, only here for backward compatability.
   maxthr - Ignored, only here for backward compatability.
*/
               XrdPosixXrootd(int maxfd=255, int maxdir=0, int maxthr=0);
              ~XrdPosixXrootd();

private:

static void                  initEnv();
static void                  initEnv(char *eData);
static void                  initEnv(XrdOucEnv &, const char *, long long &);
static int                   Fault(XrdPosixFile *fp, int ecode);
static void                  initStat(struct stat *buf);
static void                  initXdev(dev_t &st_dev, dev_t &st_rdev);

static XrdOucCache   *myCache;
static int            baseFD;
static int            initDone;
};
#endif
