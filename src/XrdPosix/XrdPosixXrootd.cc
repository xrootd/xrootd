/******************************************************************************/
/*                                                                            */
/*                     X r d P o s i x X r o o t d . c c                      */
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
/******************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdOuc/XrdOucCacheDram.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include "XrdPosix/XrdPosixAdmin.hh"
#include "XrdPosix/XrdPosixCallBack.hh"
#include "XrdPosix/XrdPosixDir.hh"
#include "XrdPosix/XrdPosixFile.hh"
#include "XrdPosix/XrdPosixMap.hh"
#include "XrdPosix/XrdPosixXrootd.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

namespace XrdPosixGlobals
{
XrdScheduler  *schedP = 0;
};

XrdOucCache   *XrdPosixXrootd::myCache  =  0;
int            XrdPosixXrootd::baseFD    = 0;
int            XrdPosixXrootd::initDone  = 0;
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixXrootd::XrdPosixXrootd(int fdnum, int dirnum, int thrnum)
{
   static XrdSysMutex myMutex;

// Only static fields are initialized here. We need to do this only once!
//
   myMutex.Lock();
   if (initDone) {myMutex.UnLock(); return;}
   initDone = 1;
   myMutex.UnLock();

// Initialize environment if not done before. To avoid static initialization
// dependencies, we need to do it once but we must be the last ones to do it
// before any library routines are called.
//
   initEnv();

// Initialize file tracking
//
   baseFD = XrdPosixObject::Init(fdnum);
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdPosixXrootd::~XrdPosixXrootd()
{

// Shutdown processing
//
   XrdPosixObject::Shutdown();
   initDone = 0;
}
 
/******************************************************************************/
/*                                A c c e s s                                 */
/******************************************************************************/
  
int XrdPosixXrootd::Access(const char *path, int amode)
{
   XrdPosixAdmin admin(path);
   mode_t stMode;
   bool   aOK = true;

// Issue the stat and verify that all went well
//
   if (!admin.Stat(&stMode)) return -1;

// Translate the mode bits
//
   if (amode & R_OK && !(stMode & S_IRUSR)) aOK = 0;
   if (amode & W_OK && !(stMode & S_IWUSR)) aOK = 0;
   if (amode & X_OK && !(stMode & S_IXUSR)) aOK = 0;

// All done
//
   if (aOK) return 0;
   errno = EACCES;
   return -1;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

int XrdPosixXrootd::Close(int fildes)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFile *fP;
   int ret;

   if (!(fP = XrdPosixObject::ReleaseFile(fildes)))
      {errno = EBADF; return -1;}

   if (fP->XCio->ioActive())
   {
      if (XrdPosixGlobals::schedP )
      {
         XrdPosixGlobals::schedP->Schedule(fP);
      }
      else {
         pthread_t tid;
         XrdSysThread::Run(&tid, XrdPosixFile::DelayedDestroy, fP, 0, "PosixFileDestroy");
      }
      return 0;
   }
   else
   {
      ret = fP->Close(Status);
      delete fP;
      return (ret ? 0 : XrdPosixMap::Result(Status));
   }
}

/******************************************************************************/
/*                              C l o s e d i r                               */
/******************************************************************************/

int XrdPosixXrootd::Closedir(DIR *dirp)
{
   XrdPosixDir *dP;
   int fildes = XrdPosixDir::dirNo(dirp);

// Get the directory object
//
   if (!(dP = XrdPosixObject::ReleaseDir(fildes)))
      {errno = EBADF; return -1;}

// Deallocate the directory
//
   delete dP;
   return 0;
}

/******************************************************************************/
/*                              e n d P o i n t                               */
/******************************************************************************/
  
int XrdPosixXrootd::endPoint(int FD, char *Buff, int Blen)
{
   XrdPosixFile *fp;
   int uPort;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(FD))) return 0;

// Make sure url is valid
//
   if (!(fp->clFile.IsOpen()))
      {fp->UnLock(); return -ENOTCONN;}

// Make sure we can fit result in the buffer
//
   std::string dataServer;
   fp->clFile.GetProperty( "DataServer", dataServer );
   XrdCl::URL dataServerUrl = dataServer;

   if (dataServer.size() >= (uint32_t)Blen)
      {fp->UnLock(); return -ENAMETOOLONG;}

// Copy the data server location
//
   strcpy(Buff, dataServer.c_str());

// Get the port and return it
//
   uPort = dataServerUrl.GetPort();
   fp->UnLock();
   return uPort;
}

/******************************************************************************/
/*                                 F s t a t                                  */
/******************************************************************************/

int     XrdPosixXrootd::Fstat(int fildes, struct stat *buf)
{
   XrdPosixFile *fp;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Return what little we can
//
   initStat(buf);
   buf->st_size   = fp->mySize;
   buf->st_atime  = buf->st_mtime = buf->st_ctime = fp->myMtime;
   buf->st_blocks = buf->st_size/512+1;
   buf->st_ino    = fp->myInode;
   buf->st_mode   = fp->myMode;

// All done
//
   fp->UnLock();
   return 0;
}
  
/******************************************************************************/
/*                                 F s y n c                                  */
/******************************************************************************/
  
int XrdPosixXrootd::Fsync(int fildes)
{
   XrdPosixFile *fp;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Do the sync
//
   if (fp->XCio->Sync() < 0) return Fault(fp, errno);
   fp->UnLock();
   return 0;
}
  
/******************************************************************************/
/*                             F t r u n c a t e                              */
/******************************************************************************/
  
int XrdPosixXrootd::Ftruncate(int fildes, off_t offset)
{
   XrdPosixFile *fp;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Do the trunc
//
   if (fp->XCio->Trunc(offset) < 0) return Fault(fp, errno);
   fp->UnLock();
   return 0;
}

/******************************************************************************/
/*                              G e t x a t t r                               */
/******************************************************************************/

#ifndef ENOATTR
#define ENOATTR ENOTSUP
#endif

long long XrdPosixXrootd::Getxattr (const char *path, const char *name, 
                                    void *value, unsigned long long size)
{
  XrdPosixAdmin admin(path);
  XrdCl::QueryCode::Code reqCode;
  int vsize = static_cast<int>(size);

// Check if user just wants the maximum length needed
//
   if (size == 0) return 1024;

// Check if we support the query
//
   if (name)
      {     if (!strcmp(name,"xroot.cksum")) reqCode=XrdCl::QueryCode::Checksum;
       else if (!strcmp(name,"xroot.space")) reqCode=XrdCl::QueryCode::Space;
       else if (!strcmp(name,"xroot.xattr")) reqCode=XrdCl::QueryCode::XAttr;
       else {errno = ENOATTR; return -1;}
      }else {errno = EINVAL;  return -1;}

// Stat the file first to allow vectoring of the request to the right server
//
   if (!admin.Stat()) return -1;

// Return the result
//
  return admin.Query(reqCode, value, vsize);
}
  
/******************************************************************************/
/*                                 L s e e k                                  */
/******************************************************************************/
  
off_t   XrdPosixXrootd::Lseek(int fildes, off_t offset, int whence)
{
   XrdPosixFile *fp;
   long long curroffset;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Set the new offset
//
   if (whence == SEEK_SET) curroffset = fp->setOffset(offset);
      else if (whence == SEEK_CUR) curroffset = fp->addOffset(offset);
              else if (whence == SEEK_END)
                      curroffset = fp->setOffset(fp->mySize+offset);
                      else return Fault(fp, EINVAL);

// All done
//
   fp->UnLock();
   return curroffset;
}
  
/******************************************************************************/
/*                                 M k d i r                                  */
/******************************************************************************/

int XrdPosixXrootd::Mkdir(const char *path, mode_t mode)
{
  XrdPosixAdmin admin(path);

// Make sure the admin is OK
//
  if (!admin.isOK()) return -1;

// Issue the mkdir
//
   return XrdPosixMap::Result(admin.Xrd.MkDir(admin.Url.GetPathWithParams(),
                                              XrdCl::MkDirFlags::None,
                                              XrdPosixMap::Mode2Access(mode))
                                             );
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdPosixXrootd::Open(const char *path, int oflags, mode_t mode,
                         XrdPosixCallBack *cbP)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFile *fp;
   XrdCl::Access::Mode     XOmode = XrdCl::Access::None;
   XrdCl::OpenFlags::Flags XOflags;
   int Opts;

// Translate R/W and R/O flags
//
   if (oflags & (O_WRONLY | O_RDWR))
      {Opts    = XrdPosixFile::isUpdt;
       XOflags = XrdCl::OpenFlags::Update;
      } else {
       Opts    = 0;
       XOflags = XrdCl::OpenFlags::Read;
      }

// Pass along the stream flag
//
   if (oflags & isStream)
      {if (XrdPosixObject::CanStream()) Opts |= XrdPosixFile::isStrm;
          else {errno = EMFILE; return -1;}
      }

// Translate create vs simple open. Always make dirpath on create!
//
   if (oflags & O_CREAT)
      {XOflags |= (oflags & O_EXCL ? XrdCl::OpenFlags::New
                                   : XrdCl::OpenFlags::Delete);
       XOflags |= XrdCl::OpenFlags::MakePath;
       XOmode   = XrdPosixMap::Mode2Access(mode);
      }
      else if (oflags & O_TRUNC && Opts & XrdPosixFile::isUpdt)
              XOflags |= XrdCl::OpenFlags::Delete;

// Allocate the new file object
//
   if (!(fp = new XrdPosixFile(path, cbP, Opts)))
      {errno = EMFILE;
       return -1;
      }

// Open the file (sync or async)
//
   if (!cbP) Status = fp->clFile.Open((std::string)path, XOflags, XOmode);
      else   Status = fp->clFile.Open((std::string)path, XOflags, XOmode,
                                      (XrdCl::ResponseHandler *)fp);

// If we failed, return the reason
//
   if (!Status.IsOK())
      {delete fp;
       return XrdPosixMap::Result(Status);
      }

// Assign a file descriptor to this file
//
   if (!(fp->AssignFD(oflags & isStream)))
      {delete fp;
       errno = EMFILE;
       return -1;
      }

// Finalize the open (this gets the stat info). For async opens, the
// finalization is defered until the callback happens.
//
   if (cbP) {errno = EINPROGRESS; return -1;}
   if (fp->Finalize(Status)) return fp->FDNum();
   return XrdPosixMap::Result(Status);
}

/******************************************************************************/
/*                               O p e n d i r                                */
/******************************************************************************/
  
DIR* XrdPosixXrootd::Opendir(const char *path)
{
   XrdPosixDir *dP;
   DIR *dirP;
   int rc;

// Get a new directory object
//
   if (!(dP = new XrdPosixDir(path)))
      {errno = ENOMEM; return (DIR *)0;}

// Assign a file descriptor to this file
//
   if (!(dP->AssignFD()))
      {delete dP;
       errno = EMFILE;
       return (DIR *)0;
      }

// Open the directory
//
   if ((dirP = dP->Open())) return dirP;

// We failed
//
   rc = errno;
   delete dP;
   errno = rc;
   return (DIR *)0;
}

/******************************************************************************/
/*                                 P r e a d                                  */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Pread(int fildes, void *buf, size_t nbyte, off_t offset)
{
   XrdPosixFile *fp;
   long long     offs, bytes;
   int           iosz;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) return Fault(fp,EOVERFLOW);
      else iosz = static_cast<int>(nbyte);

// Issue the read
//
   offs = static_cast<long long>(offset);
   bytes = fp->XCio->Read((char *)buf, offs, (int)iosz);
   if (bytes < 0) return Fault(fp,errno);

// All went well
//
   fp->UnLock();
   return (ssize_t)bytes;
}

/******************************************************************************/
/*                                P w r i t e                                 */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Pwrite(int fildes, const void *buf, size_t nbyte, off_t offset)
{
   XrdPosixFile *fp;
   long long     offs;
   int           iosz, bytes;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) return Fault(fp,EOVERFLOW);
      else iosz = static_cast<int>(nbyte);

// Issue the write
//
   offs = static_cast<long long>(offset);
   bytes = fp->XCio->Write((char *)buf, offs, (int)iosz);
   if (bytes < 0) return Fault(fp, errno);

// All went well
//
   if (offs+iosz > (long long)fp->mySize) fp->mySize = offs + iosz;
   fp->UnLock();
   return (ssize_t)iosz;
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Read(int fildes, void *buf, size_t nbyte)
{
   XrdPosixFile *fp;
   long long     bytes;
   int           iosz;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) return Fault(fp,EOVERFLOW);
      else iosz = static_cast<int>(nbyte);

// Issue the read
//
   bytes = fp->XCio->Read((char *)buf,fp->Offset(),(int)iosz);
   if (bytes < 0) return Fault(fp, errno);

// All went well
//
   fp->addOffset(bytes);
   fp->UnLock();
   return (ssize_t)bytes;
}

/******************************************************************************/
/*                                 R e a d v                                  */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Readv(int fildes, const struct iovec *iov, int iovcnt)
{
   ssize_t bytes, totbytes = 0;
   int i;

// Return the results of the read for each iov segment
//
   for (i = 0; i < iovcnt; i++)
       {bytes = Read(fildes,(void *)iov[i].iov_base,(size_t)iov[i].iov_len);
             if (bytes > 0) totbytes += bytes;
        else if (bytes < 0) return -1;
        else                break;
       }

// All done
//
   return totbytes;
}

/******************************************************************************/
/*                                 V R e a d                                  */
/******************************************************************************/

ssize_t XrdPosixXrootd::VRead(int fildes, const XrdOucIOVec *readV, int n)
{
   XrdPosixFile *fp;
   ssize_t bytes;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Issue the read
//
   if ((bytes = fp->XCio->ReadV(readV, n)) < 0) return Fault(fp, errno);

// Return bytes read
//
   fp->UnLock();
   return bytes;
}

/******************************************************************************/
/*                                R e a d d i r                               */
/******************************************************************************/

struct dirent* XrdPosixXrootd::Readdir(DIR *dirp)
{
   dirent64 *dp64;
   dirent   *dp32; // Could be the same as dp64

   if (!(dp64 = Readdir64(dirp))) return 0;

   dp32 = (struct dirent *)dp64;
   if (dp32->d_name  != dp64->d_name)
      {dp32->d_ino    = dp64->d_ino;
#if !defined(__APPLE__) && !defined(__FreeBSD__)
       dp32->d_off     = dp64->d_off;
#endif
#ifndef __solaris__
       dp32->d_type   = dp64->d_type;
#endif
       dp32->d_reclen = dp64->d_reclen;
       strcpy(dp32->d_name, dp64->d_name);
      }
   return dp32;
}

struct dirent64* XrdPosixXrootd::Readdir64(DIR *dirp)
{
   XrdPosixDir *dP;
   dirent64 *dentP;
   int rc, fildes = XrdPosixDir::dirNo(dirp);

// Find the object
//
   if (!(dP = XrdPosixObject::Dir(fildes)))
      {errno = EBADF; return 0;}

// Get the next directory entry
//
   if (!(dentP = dP->nextEntry())) rc = dP->Status();
      else rc = 0;

// Return the appropriate result
//
   dP->UnLock();
   if (rc) errno = rc;
   return dentP;
}

/******************************************************************************/
/*                              R e a d d i r _ r                             */
/******************************************************************************/

int XrdPosixXrootd::Readdir_r(DIR *dirp,   struct dirent    *entry,
                                           struct dirent   **result)
{
   dirent64 *dp64;
   int       rc;

   if ((rc = Readdir64_r(dirp, 0, &dp64)) <= 0) {*result = 0; return rc;}

   entry->d_ino    = dp64->d_ino;
#if !defined(__APPLE__) && !defined(__FreeBSD__)
   entry->d_off    = dp64->d_off;
#endif
#ifndef __solaris__
   entry->d_type   = dp64->d_type;
#endif
   entry->d_reclen = dp64->d_reclen;
   strcpy(entry->d_name, dp64->d_name);
   *result = entry;
   return rc;
}

int XrdPosixXrootd::Readdir64_r(DIR *dirp, struct dirent64  *entry,
                                           struct dirent64 **result)
{
   XrdPosixDir *dP;
   int rc, fildes = XrdPosixDir::dirNo(dirp);

// Find the object
//
   if (!(dP = XrdPosixObject::Dir(fildes)))
      {errno = EBADF; return 0;}

// Get the next entry
//
   if (!(*result = dP->nextEntry(entry))) rc = dP->Status();
      else rc = 0;

// Return the appropriate result
//
   dP->UnLock();
   return rc;
}

/******************************************************************************/
/*                                R e n a m e                                 */
/******************************************************************************/

int XrdPosixXrootd::Rename(const char *oldpath, const char *newpath)
{
   XrdPosixAdmin admin(oldpath);
   XrdCl::URL newUrl((std::string)newpath);

// Make sure the admin is OK and the new url is valid
//
  if (!admin.isOK() || !newUrl.IsValid()) {errno = EINVAL; return -1;}

// Issue the rename
//
   return XrdPosixMap::Result(admin.Xrd.Mv(admin.Url.GetPathWithParams(),
                                           newUrl.GetPathWithParams()));
}

/******************************************************************************/
/*                            R e w i n d d i r                               */
/******************************************************************************/

void XrdPosixXrootd::Rewinddir(DIR *dirp)
{
   XrdPosixDir *dP;
   int fildes = XrdPosixDir::dirNo(dirp);

// Find the object and rewind it
//
   if ((dP = XrdPosixObject::Dir(fildes)))
      {dP->rewind();
       dP->UnLock();
      }
}

/******************************************************************************/
/*                                 R m d i r                                  */
/******************************************************************************/

int XrdPosixXrootd::Rmdir(const char *path)
{
  XrdPosixAdmin admin(path);

// Make sure the admin is OK
//
  if (!admin.isOK()) return -1;

// Issue the rmdir
//
   return XrdPosixMap::Result(admin.Xrd.RmDir(admin.Url.GetPathWithParams()));
}

/******************************************************************************/
/*                                S e e k d i r                               */
/******************************************************************************/

void XrdPosixXrootd::Seekdir(DIR *dirp, long loc)
{
   XrdPosixDir *dP;
   int fildes = XrdPosixDir::dirNo(dirp);

// Find the object
//
   if (!(dP = XrdPosixObject::Dir(fildes))) return;

// Sets the current directory position
//
   if (dP->Unread() && !(dP->Open()))
      {if (loc >= dP->getEntries()) loc = dP->getEntries();
          else if (loc < 0) loc = 0;
       dP->setOffset(loc);
      }
   dP->UnLock();
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/
  
int XrdPosixXrootd::Stat(const char *path, struct stat *buf)
{
   XrdPosixAdmin admin(path);
   size_t stSize;
   ino_t  stId;
   time_t stMtime;
   mode_t stFlags;

// Issue the stat and verify that all went well
//
   if (!admin.Stat(&stFlags, &stMtime, &stSize, &stId)) return -1;

// Return what little we can
//
   initStat(buf);
   buf->st_size   = stSize;
   buf->st_blocks = stSize/512+1;
   buf->st_atime  = buf->st_mtime = buf->st_ctime = stMtime;
   buf->st_ino    = stId;
   buf->st_mode   = stFlags;
   return 0;
}

/******************************************************************************/
/*                                S t a t f s                                 */
/******************************************************************************/
  
int XrdPosixXrootd::Statfs(const char *path, struct statfs *buf)
{
   struct statvfs myVfs;
   int rc;

// Issue a statvfs() call and transcribe the results
//
   if ((rc = Statvfs(path, &myVfs))) return rc;

// The vfs structure and fs structures should be size compatible (not really)
//
   memset(buf, 0, sizeof(struct statfs));
   buf->f_bsize   = myVfs.f_bsize;
   buf->f_blocks  = myVfs.f_blocks;
   buf->f_bfree   = myVfs.f_bfree;
   buf->f_files   = myVfs.f_files;
   buf->f_ffree   = myVfs.f_ffree;
#if defined(__APPLE__) || defined(__FreeBSD__)
   buf->f_iosize  = myVfs.f_frsize;
#else
   buf->f_frsize  = myVfs.f_frsize;
#endif
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
   buf->f_bavail  = myVfs.f_bavail;
#endif
#if defined(__linux__)
   buf->f_namelen = myVfs.f_namemax;
#elif defined(__FreeBSD__)
   buf->f_namemax = myVfs.f_namemax;
#endif
   return 0;
}

/******************************************************************************/
/*                               S t a t v f s                                */
/******************************************************************************/
  
int XrdPosixXrootd::Statvfs(const char *path, struct statvfs *buf)
{
   static const int szVFS = sizeof(buf->f_bfree);
   static const long long max32 = 0x7fffffffLL;

   XrdPosixAdmin       admin(path);
   XrdCl::StatInfoVFS *vfsStat;

   long long rwFree, ssFree, rwBlks;
   int       rwNum, ssNum, rwUtil, ssUtil;

// Make sure we connected
//
   if (!admin.isOK()) return -1;

// Issue the statfvs call
//
   if (XrdPosixMap::Result(admin.Xrd.StatVFS(admin.Url.GetPathWithParams(),
                                             vfsStat)) < 0) return -1;

// Extract out the information
//
   rwNum  = static_cast<int>(vfsStat->GetNodesRW());
   rwFree = (long long)vfsStat->GetFreeRW();
   rwUtil = static_cast<int>(vfsStat->GetUtilizationRW());
   ssNum  = static_cast<int>(vfsStat->GetNodesStaging());
   ssFree = (long long)vfsStat->GetFreeStaging();
   ssUtil = static_cast<int>(vfsStat->GetUtilizationStaging());
   delete vfsStat;

// Calculate number of blocks
//
   if (rwUtil == 0) rwBlks = rwFree;
      else if (rwUtil >= 100) rwBlks = 0;
              else rwBlks = rwFree * (100 / (100 - rwUtil));
   if (ssUtil == 0) rwBlks += ssFree;
      else if (ssUtil < 100) rwBlks += ssFree * (100 / (100 - ssUtil));

// Scale units to what will fit here (we can have a 32-bit or 64-bit struct)
//
   if (szVFS < 8)
      {if (rwBlks > max32) rwBlks = max32;
       if (rwFree > max32) rwFree = max32;
       if (ssFree > max32) ssFree = max32;
      }

// Return what little we can
//
   memset(buf, 0, sizeof(struct statfs));
   buf->f_bsize   = 1024*1024;
   buf->f_frsize  = 1024*1024;
   buf->f_blocks  = static_cast<fsblkcnt_t>(rwBlks);
   buf->f_bfree   = static_cast<fsblkcnt_t>(rwFree + ssFree);
   buf->f_bavail  = static_cast<fsblkcnt_t>(rwFree);
   buf->f_ffree   = rwNum + ssNum;
   buf->f_favail  = rwNum;
   buf->f_namemax = 255;    // The best we are going to do here
   buf->f_flag    = (rwNum == 0 ? ST_RDONLY|ST_NOSUID : ST_NOSUID);
   return 0;
}

/******************************************************************************/
/*                                T e l l d i r                               */
/******************************************************************************/

long XrdPosixXrootd::Telldir(DIR *dirp)
{
   XrdPosixDir *dP;
   long pos;
   int fildes = XrdPosixDir::dirNo(dirp);

// Find the object
//
   if (!(dP = XrdPosixObject::Dir(fildes)))
      {errno = EBADF; return 0;}

// Tell the current directory location
//
   pos = dP->getOffset();
   dP->UnLock();
   return pos;
}

/******************************************************************************/
/*                              T r u n c a t e                               */
/******************************************************************************/
  
int XrdPosixXrootd::Truncate(const char *path, off_t Size)
{
  XrdPosixAdmin admin(path);
  uint64_t tSize = static_cast<uint64_t>(Size);

// Make sure the admin is OK
//
  if (!admin.isOK()) return -1;

// Issue the truncate
//
   return XrdPosixMap::Result(admin.Xrd.Truncate(admin.Url.GetPathWithParams(),
                                                 tSize));
}

/******************************************************************************/
/*                                 U n l i n k                                */
/******************************************************************************/

int XrdPosixXrootd::Unlink(const char *path)
{
  XrdPosixAdmin admin(path);

// Make sure the admin is OK
//
  if (!admin.isOK()) return -1;

// Issue the UnLink
//
   return XrdPosixMap::Result(admin.Xrd.Rm(admin.Url.GetPathWithParams()));
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Write(int fildes, const void *buf, size_t nbyte)
{
   XrdPosixFile *fp;
   int           iosz, bytes;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) return Fault(fp,EOVERFLOW);
      else iosz = static_cast<int>(nbyte);

// Issue the write
//
   bytes = fp->XCio->Write((char *)buf,fp->Offset(),(int)iosz);
   if (bytes < 0) return Fault(fp, errno);

// All went well
//
   fp->addOffset(iosz, 1);
   fp->UnLock();
   return (ssize_t)iosz;
}
 
/******************************************************************************/
/*                                W r i t e v                                 */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Writev(int fildes, const struct iovec *iov, int iovcnt)
{
   ssize_t totbytes = 0;
   int i;

// Return the results of the write for each iov segment
//
   for (i = 0; i < iovcnt; i++)
       {if (!Write(fildes,(void *)iov[i].iov_base,(size_t)iov[i].iov_len))
           return -1;
        totbytes += iov[i].iov_len;
       }

// All done
//
   return totbytes;
}
  
/******************************************************************************/
/*                               i n i t E n v                                */
/******************************************************************************/
  
void XrdPosixXrootd::initEnv()
{
   char *evar;

// Establish our internal debug value (rather modest)
//
   if ((evar = getenv("XRDPOSIX_CACHE")) && *evar > '0')
      XrdPosixMap::SetDebug(true);

// Now we must check if we have a new cache over-ride
//
   if ((evar = getenv("XRDPOSIX_CACHE")) && *evar) initEnv(evar);
      else if (myCache) {char ebuf[] = {0};        initEnv(ebuf);}
}

/******************************************************************************/

// Parse options specified as a cgi string (i.e. var=val&var=val&...). Vars:

// aprcalc=n   - bytes at which to recalculate preread performance
// aprminp     - auto preread min read pages
// aprperf     - auto preread performance
// aprtrig=n   - auto preread min read length   (can be suffized in k, m, g).
// cachesz=n   - the size of the cache in bytes (can be suffized in k, m, g).
// debug=n     - debug level (0 off, 1 low, 2 medium, 3 high).
// max2cache=n - maximum read to cache          (can be suffized in k, m, g).
// maxfiles=n  - maximum number of files to support.
// mode={c|s}  - running as a client (default) or server.
// optlg=1     - log statistics
// optpr=1     - enable pre-reads
// optsf=<val> - optimize structured file: 1 = all, 0 = off, .<sfx> specific
// optwr=1     - cache can be written to.
// pagesz=n    - individual byte size of a page (can be suffized in k, m, g).
//

void XrdPosixXrootd::initEnv(char *eData)
{
   static XrdOucCacheDram dramCache;
   XrdOucEnv theEnv(eData);
   XrdOucCache::Parms myParms;
   XrdOucCacheIO::aprParms apParms;
   long long Val;
   int isRW = 0;
   char * tP;

// Get numeric type variable (errors force a default)
//
   initEnv(theEnv, "aprcalc",   Val); if (Val >= 0) apParms.prRecalc  = Val;
   initEnv(theEnv, "aprminp",   Val); if (Val >= 0) apParms.minPages  = Val;
   initEnv(theEnv, "aprperf",   Val); if (Val >= 0) apParms.minPerf   = Val;
   initEnv(theEnv, "aprtrig",   Val); if (Val >= 0) apParms.Trigger   = Val;
   initEnv(theEnv, "cachesz",   Val); if (Val >= 0) myParms.CacheSize = Val;
   initEnv(theEnv, "maxfiles",  Val); if (Val >= 0) myParms.MaxFiles  = Val;
   initEnv(theEnv, "max2cache", Val); if (Val >= 0) myParms.Max2Cache = Val;
   initEnv(theEnv, "pagesz",    Val); if (Val >= 0) myParms.PageSize  = Val;

// Get Debug setting
//
   if ((tP = theEnv.Get("debug")))
      {if (*tP >= '0' && *tP <= '3') myParms.Options |= (*tP - '0');
          else cerr <<"XrdPosix: 'XRDPOSIX_CACHE=debug=" <<tP <<"' is invalid." <<endl;
      }

// Get Mode
//
   if ((tP = theEnv.Get("mode")))
      {if (*tP == 's') myParms.Options |= XrdOucCache::isServer;
          else if (*tP != 'c') cerr <<"XrdPosix: 'XRDPOSIX_CACHE=mode=" <<tP
                                    <<"' is invalid." <<endl;
      }

// Get the structured file option
//
   if ((tP = theEnv.Get("optsf")) && *tP && *tP != '0')
      {     if (*tP == '1') myParms.Options |= XrdOucCache::isStructured;
       else if (*tP == '.') {XrdPosixFile::sfSFX = strdup(tP);
                             XrdPosixFile::sfSLN = strlen(tP);
                            }
       else cerr <<"XrdPosix: 'XRDPOSIX_CACHE=optfs=" <<tP
                 <<"' is invalid." <<endl;
      }

// Get final options, any non-zero value will do here
//
   if ((tP = theEnv.Get("optlg")) && *tP && *tP != '0')
      myParms.Options |= XrdOucCache::logStats;
   if ((tP = theEnv.Get("optpr")) && *tP && *tP != '0')
      myParms.Options |= XrdOucCache::canPreRead;
   if ((tP = theEnv.Get("optwr")) && *tP && *tP != '0') isRW = 1;

// Use the default cache if one was not provided
//
   if (!myCache) myCache = &dramCache;

// Now allocate a cache. Indicate that we already serialize the I/O to avoid
// additional but unnecessary locking.
//
   myParms.Options |= XrdOucCache::Serialized;
   if (!(XrdPosixFile::CacheR = myCache->Create(myParms, &apParms)))
      cerr <<"XrdPosix: " <<strerror(errno) <<" creating cache." <<endl;
      else {if (isRW) XrdPosixFile::CacheW = XrdPosixFile::CacheR;}
}

/******************************************************************************/

void XrdPosixXrootd::initEnv(XrdOucEnv &theEnv, const char *vName, long long &Dest)
{
   char *eP, *tP;

// Extract variable
//
   Dest = -1;
   if (!(tP = theEnv.Get(vName)) || !(*tP)) return;

// Convert the value
//
   errno = 0;
   Dest = strtoll(tP, &eP, 10);
   if (Dest > 0 || (!errno && tP != eP))
      {if (!(*eP)) return;
            if (*eP == 'k' || *eP == 'K') Dest *= 1024LL;
       else if (*eP == 'm' || *eP == 'M') Dest *= 1024LL*1024LL;
       else if (*eP == 'g' || *eP == 'G') Dest *= 1024LL*1024LL*1024LL;
       else if (*eP == 't' || *eP == 'T') Dest *= 1024LL*1024LL*1024LL*1024LL;
       else eP--;
       if (*(eP+1))
          {cerr <<"XrdPosix: 'XRDPOSIX_CACHE=" <<vName <<'=' <<tP
                             <<"' is invalid." <<endl;
           Dest = -1;
          }
      }
}

/******************************************************************************/
/*                             i s X r o o t d D i r                          */
/******************************************************************************/

bool XrdPosixXrootd::isXrootdDir(DIR *dirp)
{
   XrdPosixDir *dP;
   int fildes;

   if (!dirp) return false;
   fildes = XrdPosixDir::dirNo(dirp);

   if (!myFD(fildes) || !(dP = XrdPosixObject::Dir(fildes))) return false;

   dP->UnLock();
   return true;
}

/******************************************************************************/
/*                                  m y F D                                   */
/******************************************************************************/

bool XrdPosixXrootd::myFD(int fd)
{
   return XrdPosixObject::Valid(fd);
}
  
/******************************************************************************/
/*                           Q u e r y C h k s u m                            */
/******************************************************************************/
  
int XrdPosixXrootd::QueryChksum(const char *path,  time_t &Mtime,
                                      char *value, int     vsize)
{
   XrdPosixAdmin admin(path);

// Stat the file first to allow vectoring of the request to the right server
//
   if (!admin.Stat(0, &Mtime)) return -1;

// Now we can get the checksum as we have landed on the right server
//
   return admin.Query(XrdCl::QueryCode::Checksum, value, vsize);
}
  
/******************************************************************************/
/*                           Q u e r y O p a q u e                            */
/******************************************************************************/
  
long long XrdPosixXrootd::QueryOpaque(const char *path, char *value, int size)
{
   XrdPosixAdmin admin(path);

// Stat the file first to allow vectoring of the request to the right server
//
   if (!admin.Stat()) return -1;

// Now we can get the checksum as we have landed on the right server
//
   return admin.Query(XrdCl::QueryCode::OpaqueFile, value, size);
}

/******************************************************************************/
/*                              s e t C a c h e                               */
/******************************************************************************/

void XrdPosixXrootd::setCache(XrdOucCache *cP)
{
     myCache = cP;
}
  
/******************************************************************************/
/*                              s e t D e b u g                               */
/******************************************************************************/

void XrdPosixXrootd::setDebug(int val, bool doDebug)
{
   const std::string dbgType[] = {"Info", "Warning", "Error", "Debug", "Dump"};

// The default is none but once set it cannot be unset in the client
//
   if (val > 0)
      {if (doDebug) val = 4;
          else if (val > 5) val = 5;
       XrdCl::DefaultEnv::SetLogLevel(dbgType[val-1]);
      }

// Now set the internal one which can be toggled
//
   XrdPosixMap::SetDebug(val > 0);
}
  
/******************************************************************************/
/*                                s e t E n v                                 */
/******************************************************************************/

void XrdPosixXrootd::setEnv(const char *kword, int kval)
{
   XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();

// Set the env value
//
   env->PutInt((std::string)kword, kval);
}
  
/******************************************************************************/
/*                               s e t I P V 4                                */
/******************************************************************************/

void XrdPosixXrootd::setIPV4(bool usev4)
{
   const char *ipmode = (usev4 ? "IPv4" : "IPAll");
   XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();

// Set the env value
//
   env->PutString((std::string)"NetworkStack", (const std::string)ipmode);
}
  
/******************************************************************************/
/*                              s e t S c h e d                               */
/******************************************************************************/

void XrdPosixXrootd::setSched(XrdScheduler *sP)
{
    XrdPosixGlobals::schedP = sP;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 F a u l t                                  */
/******************************************************************************/

int XrdPosixXrootd::Fault(XrdPosixFile *fp, int ecode)
{
   fp->UnLock();
   errno = ecode;
   return -1;
}

/******************************************************************************/
/*                              i n i t S t a t                               */
/******************************************************************************/

void XrdPosixXrootd::initStat(struct stat *buf)
{
   static int initStat = 0;
   static dev_t st_rdev;
   static dev_t st_dev;
   static uid_t myUID = getuid();
   static gid_t myGID = getgid();

// Initialize the xdev fields. This cannot be done in the constructor because
// we may not yet have resolved the C-library symbols.
//
   if (!initStat) {initStat = 1; initXdev(st_dev, st_rdev);}
   memset(buf, 0, sizeof(struct stat));

// Preset common fields
//
   buf->st_blksize= 64*1024;
   buf->st_dev    = st_dev;
   buf->st_rdev   = st_rdev;
   buf->st_nlink  = 1;
   buf->st_uid    = myUID;
   buf->st_gid    = myGID;
}
  
/******************************************************************************/
/*                              i n i t X d e v                               */
/******************************************************************************/
  
void XrdPosixXrootd::initXdev(dev_t &st_dev, dev_t &st_rdev)
{
   static dev_t tDev, trDev;
   static bool aOK = false;
   struct stat buf;

// Get the device id for /tmp used by stat()
//
   if (aOK) {st_dev = tDev; st_rdev = trDev;}
      else if (stat("/tmp", &buf)) {st_dev = 0; st_rdev = 0;}
              else {st_dev  = tDev  = buf.st_dev;
                    st_rdev = trDev = buf.st_rdev;
                    aOK = true;
                   }
}
