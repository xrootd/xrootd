/******************************************************************************/
/*                                                                            */
/*                  X r d P o s i x P r e l o a d 3 2 . c c                   */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//           $Id$

#undef  _LARGEFILE_SOURCE
#undef  _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32

#include <dirent.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "XrdPosix/XrdPosixExtern.hh"
#include "XrdPosix/XrdPosixLinkage.hh"
#include "XrdPosix/XrdPosixStream.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
 
/******************************************************************************/
/*                   G l o b a l   D e c l a r a t i o n s                    */
/******************************************************************************/
  
extern XrdPosixLinkage Xunix;

extern XrdPosixRootVec xinuX;

extern XrdPosixStream  streamX;
 
/******************************************************************************/
/*               6 4 - t o 3 2   B i t   C o n v e r s i o n s                */
/******************************************************************************/
/******************************************************************************/
/*                   X r d P o s i x _ C o p y D i r e n t                    */
/******************************************************************************/
  
int XrdPosix_CopyDirent(struct dirent *dent, struct dirent64 *dent64)
{
  const long long LLMask = 0xffffffff00000000LL;

  if ((dent64->d_ino    & LLMask)
  ||  (dent64->d_off    & LLMask)) {errno = EOVERFLOW; return EOVERFLOW;}

  dent->d_ino    = dent64->d_ino;
  dent->d_off    = dent64->d_off;
  dent->d_reclen = dent64->d_reclen;
  strcpy(dent->d_name, dent64->d_name);
  return 0;
}

/******************************************************************************/
/*                     X r d P o s i x _ C o p y S t a t                      */
/******************************************************************************/

int XrdPosix_CopyStat(struct stat *buf, struct stat64 &buf64)
{
  const long long LLMask = 0xffffffff00000000LL;
  const      int  INTMax = 0x7fffffff;

  if (buf64.st_size   & LLMask)
     if (buf64.st_mode & S_IFREG || buf64.st_mode & S_IFDIR)
        {errno = EOVERFLOW; return -1;}
        else buf->st_size   = INTMax;
     else buf->st_size =  buf64.st_size;  /* 64: File size in bytes */

      buf->st_ino   = buf64.st_ino    & LLMask ? INTMax : buf64.st_ino;
      buf->st_blocks= buf64.st_blocks & LLMask ? INTMax : buf64.st_blocks;
      buf->st_mode  = buf64.st_mode;     /*     File mode (see mknod(2)) */
      buf->st_dev   = buf64.st_dev;
      buf->st_rdev  = buf64.st_rdev;     /*     ID of device */
      buf->st_nlink = buf64.st_nlink;    /*     Number of links */
      buf->st_uid   = buf64.st_uid;      /*     User ID of the file's owner */
      buf->st_gid   = buf64.st_gid;      /*     Group ID of the file's group */
      buf->st_atime = buf64.st_atime;    /*     Time of last access */
      buf->st_mtime = buf64.st_mtime;    /*     Time of last data modification */
      buf->st_ctime = buf64.st_ctime;    /*     Time of last file status change */
      buf->st_blksize=buf64.st_blksize;  /*     Preferred I/O block size */
  return 0;
}

/******************************************************************************/
/*                                 c r e a t                                  */
/******************************************************************************/
  
extern "C"
{
int     creat(const char *path, mode_t mode)
{
        return xinuX.Open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
}
  
/******************************************************************************/
/*                                 f o p e n                                  */
/******************************************************************************/
  
extern "C"
{
FILE  *fopen(const char *path, const char *mode)
{

  return xinuX.isMyPath(path)
         ? streamX.Fopen(path, mode)
         :   Xunix.Fopen(path, mode);
}
}

/******************************************************************************/
/*                                 f s t a t                                  */
/******************************************************************************/

extern "C"
{
#if defined __linux__ && __GNUC__ && __GNUC__ >= 2
int  __fxstat(int ver, int fildes, struct stat *buf)
#else
int     fstat(         int fildes, struct stat *buf)
#endif
{
  struct stat64 buf64;
  int rc;

#ifdef __linux__
  if (fildes < XrdPosixFD) return Xunix.Fstat(ver, fildes, buf);
#else
  if (fildes < XrdPosixFD) return Xunix.Fstat(     fildes, buf);
#endif
  if ((rc = xinuX.Fstat(fildes, (struct stat *)&buf64))) return rc;
  return XrdPosix_CopyStat(buf, buf64);
}
}

/******************************************************************************/
/*                                 l s e e k                                  */
/******************************************************************************/
  
extern "C"
{
off_t   lseek(int fildes, off_t offset, int whence)
{
        return (fildes >= XrdPosixFD) 
               ? xinuX.Lseek(fildes, offset, whence)
               : Xunix.Lseek(fildes, offset, whence);
}
}

/******************************************************************************/
/*                                 l s t a t                                  */
/******************************************************************************/

extern "C"
{
#if defined __GNUC__ && __GNUC__ >= 2
int     __xlstat(int ver, const char *path, struct stat *buf)
#else
int        lstat(         const char *path, struct stat *buf)
#endif
{
  struct stat64 buf64;
  int rc;

  if (!xinuX.isMyPath(path))
#ifdef __linux__
     return Xunix.Lstat(ver, path, buf);
#else
     return Xunix.Lstat(     path, buf);
#endif

  if ((rc = xinuX.Stat(path, (struct stat *)&buf64))) return rc;
  return XrdPosix_CopyStat(buf, buf64);
}
}

/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/
  
extern "C"
{
int     open(const char *path, int oflag, ...)
{
   va_list ap;
   int mode;

   va_start(ap, oflag);
   mode = va_arg(ap, int);
   va_end(ap);
   return xinuX.Open(path, oflag, mode);
}
}

/******************************************************************************/
/*                                 p r e a d                                  */
/******************************************************************************/
  
extern "C"
{
ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset)
{
        return (fildes >= XrdPosixFD)
               ? xinuX.Pread(fildes, buf, nbyte, offset)
               : Xunix.Pread(fildes, buf, nbyte, offset);
}
}

/******************************************************************************/
/*                               r e a d d i r                                */
/******************************************************************************/

extern "C"
{
struct dirent* readdir(DIR *dirp)
{
   struct dirent *dent;

   if (!(dent = xinuX.Readdir(dirp))) return 0;

   if (XrdPosix_CopyDirent(dent, (struct dirent64 *)dent)) return 0;

   return dent;
}
}

/******************************************************************************/
/*                             r e a d d i r _ r                              */
/******************************************************************************/
  
extern "C"
{
int     readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
   char buff[sizeof(struct dirent64) + 2048];
   struct dirent64 *dent64 = (struct dirent64 *)buff;
   struct dirent   *mydirent;
   int rc;

   if ((rc = xinuX.Readdir_r(dirp, (struct dirent *)dent64, &mydirent)))
      return rc;

   if (!mydirent) {*result = 0; return 0;}
   if ((rc = XrdPosix_CopyDirent(entry, dent64))) return rc;
   *result = entry;
   return 0;
}
}

/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

extern "C"
{
#if defined __GNUC__ && __GNUC__ >= 2
int     __xstat(int ver, const char *path, struct stat *buf)
#else
int        stat(         const char *path, struct stat *buf)
#endif
{
  struct stat64 buf64;
  int rc;

  if (!xinuX.isMyPath(path))
#ifdef __linux__
     return Xunix.Stat(ver, path, buf);
#else
     return Xunix.Stat(     path, buf);
#endif
  if ((rc = xinuX.Stat(path, (struct stat *)&buf64))) return rc;
  return XrdPosix_CopyStat(buf, buf64);
}
}

/******************************************************************************/
/*                                p w r i t e                                 */
/******************************************************************************/
  
extern "C"
{
ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset)
{
        return (fildes >= XrdPosixFD) 
               ? xinuX.Pwrite(fildes, buf, nbyte, offset)
               : Xunix.Pwrite(fildes, buf, nbyte, offset);
}
}
