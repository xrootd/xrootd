/******************************************************************************/
/*                                                                            */
/*                     X r d P o s i x X r o o t d . c c                      */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdPosixXrootd.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdPosixFile
{
public:

XrdClient *XClient;

long long  Offset() {return currOffset;}

long long  addOffset(long long offs)
                    {currOffset += offs;
                     return currOffset;
                    }

long long  setOffset(long long offs)
                    {currOffset = offs;
                     return currOffset;
                    }

void         Lock() {myMutex.Lock();}
void       UnLock() {myMutex.UnLock();}

int               FD;

XrdClientStatInfo stat;

           XrdPosixFile(int fd, const char *path);
          ~XrdPosixFile();

private:

XrdOucMutex myMutex;
long long   currOffset;
};

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/
  
#define Scuttle(fp, rc) fp->UnLock(); errno = rc; return -1

#define retError(fp) {int rc = mapError(fp->XClient->LastServerResp()->status);\
                      Scuttle(fp, rc);}

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

XrdOucMutex    XrdPosixXrootd::myMutex;
XrdPosixFile **XrdPosixXrootd::myFiles  =  0;
int            XrdPosixXrootd::highFD   = -1;
int            XrdPosixXrootd::lastFD   = -1;
const int      XrdPosixXrootd::FDMask   = 0x0000ffff;
const int      XrdPosixXrootd::FDOffs   = 0x00010000;
const int      XrdPosixXrootd::FDLeft   = 0x7fff0000;
  
/******************************************************************************/
/*                          X r d P o s i x F i l e                           */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixFile::XrdPosixFile(int fd, const char *path)
{

// Allocate a new client object
//
   if (!(XClient = new XrdClient(path)))
      {stat.size = 0; return;}

// Set the FD
//
   FD = fd;
   currOffset = 0;
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdPosixFile::~XrdPosixFile()
{
   if (XClient) delete XClient;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixXrootd::XrdPosixXrootd(int fdnum)
{
   struct rlimit rlim;
   int isize;

// Compute size of table
//
   if (!getrlimit(RLIMIT_NOFILE, &rlim)) fdnum = (int)rlim.rlim_cur;
   if (fdnum > 32768) fdnum = 32768;
   isize = fdnum * sizeof(XrdPosixFile *);

// Allocate an initial table of 64 fd-type pointers
//
   if (!(myFiles = (XrdPosixFile **)malloc(isize))) lastFD = -1;
      else {memset((void *)myFiles, isize, 0); lastFD = fdnum;}

// For now, turn off the read-ahead cache
//
   EnvPutInt(NAME_READCACHESIZE, 0);
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdPosixXrootd::~XrdPosixXrootd()
{
   int i;

   if (myFiles)
      {for (i = 0; i <= highFD; i++) if (myFiles[i]) delete myFiles[i];
       free(myFiles);
      }
}
 
/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

int     XrdPosixXrootd::Close(int fildes)
{
   XrdPosixFile *fp;

// Find the file object. We tell findFP() to leave the global lock on
//
   if (!(fp = findFP(fildes, 1))) return -1;

// Deallocate the file. We have the global lock.
//
   myFiles[fp->FD] = 0;
   fp->UnLock();
   myMutex.UnLock();
   delete fp;
   return 0;
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
   if (!(fp = findFP(fildes))) return -1;

// Set the new offset
//
   if (whence == SEEK_SET) curroffset = fp->setOffset(offset);
      else if (whence == SEEK_CUR) curroffset = fp->addOffset(offset);
              else if (whence == SEEK_END)
                      curroffset = fp->setOffset(fp->stat.size+offset);
                      else {Scuttle(fp, EINVAL);}

// All done
//
   fp->UnLock();
   return curroffset;
}

/******************************************************************************/
/*                                 F s t a t                                  */
/******************************************************************************/

int     XrdPosixXrootd::Fstat(int fildes, struct stat *buf)
{
   XrdPosixFile *fp;

// Find the file object
//
   if (!(fp = findFP(fildes))) return -1;

// Clear the stat buffer (there is precious little we can return
//
   memset(buf, 0, sizeof(struct stat));
   buf->st_nlink  = 1;
   buf->st_size   = fp->stat.size;
   buf->st_atime  = buf->st_mtime = buf->st_ctime = fp->stat.modtime;
   buf->st_blocks = buf->st_size/512+1;
   buf->st_ino    = fp->stat.id;
   buf->st_mode   = mapFlags(fp->stat.flags);

// All done
//
   fp->UnLock();
   return 0;
}
  
/******************************************************************************/
/*                                 F s y n c                                  */
/******************************************************************************/
  
int     XrdPosixXrootd::Fsync(int fildes)
{
   XrdPosixFile *fp;

// Find the file object
//
   if (!(fp = findFP(fildes))) return -1;

// Do the sync
//
   if (!fp->XClient->Sync()) retError(fp);
   fp->UnLock();
   return 0;
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int     XrdPosixXrootd::Open(const char *path, int oflags, int mode)
{
   XrdPosixFile *fp;
   int retc = 0, fd, XOflags, XMode;

// Allocate a new file descriptor.
//
   myMutex.Lock();
   for (fd = 0; fd < lastFD; fd++) if (!myFiles[fd]) break;
   if (fd > lastFD || !(fp = new XrdPosixFile(fd, path)))
      {errno = EMFILE; myMutex.UnLock(); return -1;}
   myFiles[fd] = fp;
   if (fd > highFD) highFD = fd;
   myMutex.UnLock();

// Translate option bits to the appropraite values
//
   XOflags = (oflags & (O_WRONLY | O_RDWR) ? kXR_open_updt : kXR_open_read);
   if (oflags & O_CREAT) XOflags |= (oflags & O_EXCL ? kXR_new : kXR_delete);

// Translate the mode, if need be
//
   XMode = 0;
   if (mode && (oflags & O_CREAT))
      {if (mode & S_IRUSR) XMode |= kXR_ur;
       if (mode & S_IWUSR) XMode |= kXR_uw;
       if (mode & S_IXUSR) XMode |= kXR_ux;
       if (mode & S_IRGRP) XMode |= kXR_gr;
       if (mode & S_IWGRP) XMode |= kXR_gw;
       if (mode & S_IXGRP) XMode |= kXR_gx;
       if (mode & S_IROTH) XMode |= kXR_or;
      }

// Open the file
//
   if (!fp->XClient->Open(mode, XOflags)
   || (retc = fp->XClient->LastServerResp()->status) != kXR_ok)
      {myMutex.Lock();
       myFiles[fd] = 0;
       delete fp;
       myMutex.UnLock();
       errno = mapError(retc);
       return -1;
      }

// Get the file size
//
   fp->XClient->Stat(&fp->stat);

// Return the fd number
//
   return fd | FDOffs;
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
   if (!(fp = findFP(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) {Scuttle(fp,EOVERFLOW);}
      else iosz = static_cast<int>(nbyte);

// Issue the read
//
   offs = static_cast<long long>(offset);
   if (!(bytes = fp->XClient->Read(buf, offs, (int)iosz)) ) retError(fp);

// All went well
//
   fp->UnLock();
   return (ssize_t)bytes;
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
   if (!(fp = findFP(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) {Scuttle(fp,EOVERFLOW);}
      else iosz = static_cast<int>(nbyte);

// Issue the read
//
   if (!(bytes = fp->XClient->Read(buf, fp->Offset(), (int)iosz)) )
      retError(fp);

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
       {if ((bytes = Read(fildes,(void *)iov[i].iov_base,(size_t)iov[i].iov_len)))
           return -1;
        totbytes += bytes;
       }

// All done
//
   return totbytes;
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/
  
int XrdPosixXrootd::Stat(const char *path, struct stat *buf)
{
    int rc, fd;

// Open the file first
//
   if ((fd = Open(path, O_RDONLY)) >= 0)
      {rc = (Fstat(fd, buf) < 0 ? errno : 0);
       Close(fd);
       return rc;
      }

// Handle open errors
//
   if (errno != EISDIR) return -1;
   memset(buf, 0, sizeof(buf));
   buf->st_nlink  = 1;
   buf->st_size   = 512;
   buf->st_mode   = S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH | S_IFDIR;
   return 0;
}

/******************************************************************************/
/*                                P w r i t e                                 */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Pwrite(int fildes, const void *buf, size_t nbyte, off_t offset)
{
   XrdPosixFile *fp;
   long long     offs;
   int           iosz;

// Find the file object
//
   if (!(fp = findFP(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) {Scuttle(fp,EOVERFLOW);}
      else iosz = static_cast<int>(nbyte);

// Issue the write
//
   offs = static_cast<long long>(offset);
   if (!fp->XClient->Write(buf, offs, iosz)) retError(fp);

// All went well
//
   fp->UnLock();
   return (ssize_t)iosz;
}
  
/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
ssize_t XrdPosixXrootd::Write(int fildes, const void *buf, size_t nbyte)
{
   XrdPosixFile *fp;
   int           iosz;

// Find the file object
//
   if (!(fp = findFP(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) {Scuttle(fp,EOVERFLOW);}
      else iosz = static_cast<int>(nbyte);

// Issue the write
//
   if (!fp->XClient->Write(buf, fp->Offset(), iosz)) retError(fp);

// All went well
//
   fp->addOffset(iosz);
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
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                f i n d F P                                 */
/******************************************************************************/
  
XrdPosixFile *XrdPosixXrootd::findFP(int fildes, int glk)
{
   XrdPosixFile *fp;
   int fd;

// Validate the fildes
//
   fd = fildes & FDMask;
   if (fd >= lastFD || fildes < 0 || (fildes & FDLeft) != FDOffs) 
      {errno = EBADF; return (XrdPosixFile *)0;}

// Obtain the file object, if any
//
   myMutex.Lock();
   if (!(fp = myFiles[fd])) {myMutex.UnLock(); errno = EBADF; return fp;}

// Lock the object and unlock the global lock unless it is to be held
//
   fp->Lock();
   if (!glk) myMutex.UnLock();
   return fp;
}
 
/******************************************************************************/
/*                              m a p E r r o r                               */
/******************************************************************************/
  
int XrdPosixXrootd::mapError(int rc)
{
    switch(rc)
       {case kXR_NotFound:      return ENOENT;
        case kXR_NotAuthorized: return EACCES;
        case kXR_IOError:       return EIO;
        case kXR_NoMemory:      return ENOMEM;
        case kXR_NoSpace:       return ENOSPC;
        case kXR_ArgTooLong:    return ENAMETOOLONG;
        case kXR_noserver:      return EHOSTUNREACH;
        case kXR_NotFile:       return ENOTBLK;
        case kXR_isDir:         return EISDIR;
        case kXR_FSError:       return ENOSYS;
        default:                return ECANCELED;
       }
}
 
/******************************************************************************/
/*                              m a p F l a g s                               */
/******************************************************************************/
  
int XrdPosixXrootd::mapFlags(int flags)
{
   int newflags = S_IRUSR | S_IWUSR;

// Map the xroot flags to unix flags
//
   if (flags & kXR_xset) newflags |= S_IXUSR | S_IXGRP | S_IXOTH;
   if (flags & kXR_other)newflags |= S_IFBLK;
      else if (flags * kXR_isDir) newflags |= S_IFDIR;
              else newflags |= S_IFREG;
   if (flags & kXR_offline) newflags |= S_ISVTX;

   return newflags;
}
