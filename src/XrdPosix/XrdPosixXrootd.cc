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
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>

#include "XrdVersion.hh"

#include "Xrd/XrdScheduler.hh"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdOuc/XrdOucCache2.hh"
#include "XrdOuc/XrdOucCacheDram.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucPsx.hh"

#include "XrdPosix/XrdPosixAdmin.hh"
#include "XrdPosix/XrdPosixCacheBC.hh"
#include "XrdPosix/XrdPosixCallBack.hh"
#include "XrdPosix/XrdPosixConfig.hh"
#include "XrdPosix/XrdPosixDir.hh"
#include "XrdPosix/XrdPosixFile.hh"
#include "XrdPosix/XrdPosixFileRH.hh"
#include "XrdPosix/XrdPosixInfo.hh"
#include "XrdPosix/XrdPosixMap.hh"
#include "XrdPosix/XrdPosixPrepIO.hh"
#include "XrdPosix/XrdPosixTrace.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"

#include "XrdSys/XrdSysTrace.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

class XrdSysError;

namespace XrdPosixGlobals
{
XrdScheduler    *schedP    = 0;
XrdOucCache2    *theCache  = 0;
XrdOucCache     *myCache   = 0;
XrdOucCache2    *myCache2  = 0;
XrdOucName2Name *theN2N    = 0;
XrdCl::DirListFlags::Flags dlFlag = XrdCl::DirListFlags::None;
XrdSysLogger    *theLogger = 0;
XrdSysError     *eDest     = 0;
XrdSysTrace      Trace("Posix", 0,
                      (getenv("XRDPOSIX_DEBUG") ? TRACE_Debug : 0));
int              ddInterval= 30;
int              ddMaxTries= 180/30;
bool             oidsOK    = false;
};

int            XrdPosixXrootd::baseFD    = 0;
int            XrdPosixXrootd::initDone  = 0;

XrdVERSIONINFO(XrdPosix,XrdPosix);
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                               L f n P a t h                                */
/******************************************************************************/

namespace
{
class LfnPath
{
public:
const char *path;

            LfnPath(const char *who, const char *pURL, bool ponly=true)
                   {path = XrdPosixXrootPath::P2L(who, pURL, relURL, ponly);}

           ~LfnPath() {if (relURL) free(relURL);}

private:
char *relURL;
};
}
  
/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/

namespace
{
  
/******************************************************************************/
/*                             O p e n D e f e r                              */
/******************************************************************************/

int OpenDefer(XrdPosixFile           *fp,
              XrdPosixCallBack       *cbP,
              XrdCl::OpenFlags::Flags XOflags,
              XrdCl::Access::Mode     XOmode,
              bool                    isStream)
{

// Assign a file descriptor to this file
//
   if (!(fp->AssignFD(isStream)))
      {delete fp;
       errno = EMFILE;
       return -1;
      }

// Allocate a prepare I/O object to defer this open
//
   fp->PrepIO = new XrdPosixPrepIO(fp, XOflags, XOmode);

// Finalize this file object. A null argument indicates it is defered.
//
   fp->Finalize(0);

// For sync opens we just need to return the file descriptor
//
   if (!cbP) return fp->FDNum();

// For async opens do the callback here and return an inprogress
//
   cbP->Complete(fp->FDNum());
   errno = EINPROGRESS;
   return -1;
}
};
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixXrootd::XrdPosixXrootd(int fdnum, int dirnum, int thrnum)
{
   static XrdSysMutex myMutex;
   char *cfn;

// Only static fields are initialized here. We need to do this only once!
//
   myMutex.Lock();
   if (initDone) {myMutex.UnLock(); return;}
   initDone = 1;
   myMutex.UnLock();

// Initialize environment as a client or a server (it differs somewhat).
//
   if (!XrdPosixGlobals::theLogger && (cfn=getenv("XRDPOSIX_CONFIG")) && *cfn)
      {bool hush;
       if (*cfn == '+') {hush = false; cfn++;}
          else hush = (getenv("XRDPOSIX_DEBUG") == 0);
       if (*cfn)
          {XrdOucPsx psxConfig(&XrdVERSIONINFOVAR(XrdPosix), cfn);
           if (!psxConfig.ClientConfig("posix.", hush)
           ||  !XrdPosixConfig::SetConfig(psxConfig))
              {std::cerr <<"Posix: Unable to instantiate specified "
                           "configuration; program exiting!" <<std::endl;
               exit(16);
              }
          }
      }

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
   EPNAME("Close");
   XrdCl::XRootDStatus Status;
   XrdPosixFile *fP;
   bool ret;

// Map the file number to the file object. In the prcess we relese the file
// number so no one can reference this file again.
//
   if (!(fP = XrdPosixObject::ReleaseFile(fildes)))
      {errno = EBADF; return -1;}

// Close the file if there is no active I/O (possible caching). Delete the
// object if the close was successful (it might not be).
//
   if (!(fP->XCio->ioActive()) && !fP->Refs())
      {if ((ret = fP->Close(Status))) {delete fP; fP = 0;}
          else if (DEBUGON)
                  {std::string eTxt = Status.ToString();
                   DEBUG(eTxt <<" closing " <<fP->Origin());
                  }
      } else ret = true;

// If we still have a handle then we need to do a delayed delete on this
// object because either the close failed or there is still active I/O
//
   if (fP) XrdPosixFile::DelayedDestroy(fP);

// Return final result
//
   return (ret ? 0 : XrdPosixMap::Result(Status));
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
   int rc;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// First initialize the stat buffer
//
   initStat(buf);

// Check if we can get the stat information from the cache.
//
   rc = fp->XCio->Fstat(*buf);
   if (rc <= 0)
      {fp->UnLock();
       if (!rc) return 0;
       errno = -rc;
       return -1;
      }

// At this point we can call the file's Fstat() and if the file is not open
// it will be opened.
//
   rc = fp->Fstat(*buf);
   fp->UnLock();
   if (rc < 0) {errno = -rc; rc = -1;}
   return rc;
}
  
/******************************************************************************/
/*                                 F s y n c                                  */
/******************************************************************************/
  
int XrdPosixXrootd::Fsync(int fildes)
{
   XrdPosixFile *fp;
   int rc;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Do the sync
//
   if ((rc = fp->XCio->Sync()) < 0) return Fault(fp, -rc);
   fp->UnLock();
   return 0;
}
  
/******************************************************************************/
  
void XrdPosixXrootd::Fsync(int fildes, XrdPosixCallBackIO *cbp)
{
   XrdPosixFile *fp;

// Find the file object and do the sync
//
   if ((fp = XrdPosixObject::File(fildes)))
      {cbp->theFile = fp;
       fp->Ref(); fp->UnLock();
       fp->XCio->Sync(*cbp);
      } else cbp->Complete(-1);
}

/******************************************************************************/
/*                             F t r u n c a t e                              */
/******************************************************************************/
  
int XrdPosixXrootd::Ftruncate(int fildes, off_t offset)
{
   XrdPosixFile *fp;
   int rc;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) return -1;

// Do the trunc
//
   if ((rc = fp->XCio->Trunc(offset)) < 0) return Fault(fp, -rc);
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

// Set the new offset. Note that SEEK_END requires that the file be opened.
// An open may occur by calling the FSize() method via the cache pointer.
//
        if (whence == SEEK_SET) curroffset = fp->setOffset(offset);
   else if (whence == SEEK_CUR) curroffset = fp->addOffset(offset);
   else if (whence == SEEK_END)
           {curroffset = fp->XCio->FSize();
            if (curroffset < 0) return Fault(fp,static_cast<int>(-curroffset));
            curroffset = fp->setOffset(curroffset+offset);
           }
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
  XrdCl::MkDirFlags::Flags flags;

// Preferentially make the whole path unless told otherwise
//
   flags = (mode & S_ISUID ? XrdCl::MkDirFlags::None
                           : XrdCl::MkDirFlags::MakePath);

// Make sure the admin is OK
//
  if (!admin.isOK()) return -1;

// Issue the mkdir
//
   return XrdPosixMap::Result(admin.Xrd.MkDir(admin.Url.GetPathWithParams(),
                                              flags,
                                              XrdPosixMap::Mode2Access(mode))
                                             );
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdPosixXrootd::Open(const char *path, int oflags, mode_t mode,
                         XrdPosixCallBack *cbP)
{
    return Open(path, oflags, mode, cbP, 0);
}
  
int XrdPosixXrootd::Open(const char *path, int oflags, mode_t mode,
                         XrdPosixCallBack *cbP, XrdPosixInfo *infoP)
{
   EPNAME("Open");
   XrdCl::XRootDStatus Status;
   XrdPosixFile *fp;
   XrdCl::Access::Mode     XOmode = XrdCl::Access::None;
   XrdCl::OpenFlags::Flags XOflags;
   int Opts;
   bool aOK, isRO = false;

// Translate R/W and R/O flags
//
   if (oflags & (O_WRONLY | O_RDWR))
      {Opts    = XrdPosixFile::isUpdt;
       XOflags = XrdCl::OpenFlags::Update;
      } else {
       Opts    = 0;
       XOflags = XrdCl::OpenFlags::Read;
       isRO    = true;
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
   if (!(fp = new XrdPosixFile(aOK, path, cbP, Opts)))
      {errno = EMFILE;
       return -1;
      }

// Check if all went well during allocation
//
   if (!aOK) {delete fp; return -1;}

// If we have a cache, then issue a prepare as the cache may want to defer the
// open request ans we have a lot more work to do.
//
   if (XrdPosixGlobals::myCache2)
      {int rc;
       if (infoP && isRO && OpenCache(*fp, *infoP))
          {delete fp;
           errno = 0;
           return -3;
          }
       rc = XrdPosixGlobals::myCache2->Prepare(fp->Path(), oflags, mode);
       if (rc > 0) return OpenDefer(fp, cbP, XOflags, XOmode, oflags&isStream);
       if (rc < 0) {delete fp; errno = -rc; return -1;}
      }

// Open the file (sync or async)
//
   if (!cbP) Status = fp->clFile.Open((std::string)path, XOflags, XOmode);
      else   Status = fp->clFile.Open((std::string)path, XOflags, XOmode,
                                      (XrdCl::ResponseHandler *)fp);

// If we failed, return the reason
//
   if (!Status.IsOK())
      {XrdPosixMap::Result(Status);
       int rc = errno;
       if (DEBUGON && rc != ENOENT && rc != ELOOP)
          {std::string eTxt = Status.ToString();
           DEBUG(eTxt <<" open " <<fp->Origin());
          }
       delete fp;
       errno = rc;
       return -1;
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
   if (fp->Finalize(&Status)) return fp->FDNum();
   return XrdPosixMap::Result(Status);
}
  
/******************************************************************************/
/* Private:                    O p e n C a c h e                              */
/******************************************************************************/
  
bool XrdPosixXrootd::OpenCache(XrdPosixFile &file,XrdPosixInfo &Info)
{
   EPNAME("OpenCache");
   int rc;

// Check if the full file is in the cache
//
   rc = XrdPosixGlobals::myCache2->LocalFilePath(file.Path(), Info.cachePath,
                                                 sizeof(Info.cachePath),
                                                 XrdOucCache2::ForAccess,
                                                 Info.ffReady);
   if (rc == 0)
      {Info.ffReady  = true;
       DEBUG("File in cache url=" <<Info.cacheURL);
       return true;
      }

// File is not fully in the cache
//
   Info.ffReady = false;
   return false;
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
   if (bytes < 0) return Fault(fp,-bytes);

// All went well
//
   fp->UnLock();
   return (ssize_t)bytes;
}

/******************************************************************************/
  
void XrdPosixXrootd::Pread(int fildes, void *buf, size_t nbyte, off_t offset,
                           XrdPosixCallBackIO *cbp)
{
   XrdPosixFile *fp;
   long long     offs;
   int           iosz;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) {cbp->Complete(-1); return;}

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff)
      {fp->UnLock();
       errno = EOVERFLOW;
       cbp->Complete(-1);
       return;
      }

// Prepare for the read
//
   cbp->theFile = fp;
   fp->Ref(); fp->UnLock();
   iosz = static_cast<int>(nbyte);
   offs = static_cast<long long>(offset);

// Issue the read
//
   fp->XCio->Read(*cbp, (char *)buf, offs, (int)iosz);
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
   if (bytes < 0) return Fault(fp,-bytes);

// All went well
//
   fp->UpdtSize(offs + iosz);
   fp->UnLock();
   return (ssize_t)iosz;
}

/******************************************************************************/
  
void XrdPosixXrootd::Pwrite(int fildes, const void *buf, size_t nbyte,
                            off_t offset, XrdPosixCallBackIO *cbp)
{
   XrdPosixFile *fp;
   long long     offs;
   int           iosz;

// Find the file object
//
   if (!(fp = XrdPosixObject::File(fildes))) {cbp->Complete(-1); return;}

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff)
      {fp->UnLock();
       errno = EOVERFLOW;
       cbp->Complete(-1);
       return;
      }

// Prepare for the writing
//
   cbp->theFile = fp;
   fp->Ref(); fp->UnLock();
   iosz = static_cast<int>(nbyte);
   offs = static_cast<long long>(offset);

// Issue the read
//
   fp->XCio->Write(*cbp, (char *)buf, offs, (int)iosz);
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
   if (bytes < 0) return Fault(fp,-bytes);

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
   if ((bytes = fp->XCio->ReadV(readV, n)) < 0) return Fault(fp,-bytes);

// Return bytes read
//
   fp->UnLock();
   return bytes;
}

/******************************************************************************/

void XrdPosixXrootd::VRead(int fildes, const XrdOucIOVec *readV, int n,
                           XrdPosixCallBackIO *cbp)
{
   XrdPosixFile *fp;

// Find the file object and issue read
//
   if ((fp = XrdPosixObject::File(fildes)))
      {cbp->theFile = fp;
       fp->Ref(); fp->UnLock();
       fp->XCio->ReadV(*cbp, readV, n);
      } else cbp->Complete(-1);
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
   dirent64 *dp64 = 0, d64ent;
   int       rc;

   if ((rc = Readdir64_r(dirp, &d64ent, &dp64)) || !dp64)
      {*result = 0; return rc;}

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
   if (!(dP = XrdPosixObject::Dir(fildes))) return EBADF;

// Get the next entry
//
   if (!(*result = dP->nextEntry(entry))) {rc = dP->Status(); *result = 0;}
      else {rc = 0; *result = entry;}

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

// Issue rename to he cache (it really should just deep-six both files)
//
   if (XrdPosixGlobals::theCache)
      {LfnPath oldF("rename", oldpath);
       LfnPath newF("rename", newpath);
       if (!oldF.path || !newF.path) return -1;
       XrdPosixGlobals::theCache->Rename(oldF.path, newF.path);
      }

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

// Remove directory from the cache first
//
   if (XrdPosixGlobals::theCache)
      {LfnPath rmd("rmdir", path);
       if (!rmd.path) return -1;
       XrdPosixGlobals::theCache->Rmdir(rmd.path);
      }

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
   dev_t  stRdev;
   ino_t  stId;
   time_t stMtime;
   mode_t stFlags;

// Make sure the admin is OK
//
   if (!admin.isOK()) return -1;

// Initialize the stat buffer
//
   initStat(buf);

// Check if we can get the stat informatation from the cache
//
  if (XrdPosixGlobals::myCache2)
     {LfnPath statX("stat", path, false);
      if (!statX.path) return -1;
      int rc = XrdPosixGlobals::myCache2->Stat(statX.path, *buf);
      if (!rc) return 0;
      if (rc < 0) {errno = -rc; return -1;}
     }

// Issue the stat and verify that all went well
//
   if (!admin.Stat(&stFlags, &stMtime, &stSize, &stId, &stRdev)) return -1;

// Return what little we can
//
   buf->st_size   = stSize;
   buf->st_blocks = stSize/512+1;
   buf->st_atime  = buf->st_mtime = buf->st_ctime = stMtime;
   buf->st_ino    = stId;
   buf->st_rdev   = stRdev;
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
   memset(buf, 0, sizeof(struct statvfs));
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

// Truncate in the cache first
//
   if (XrdPosixGlobals::theCache)
      {LfnPath trunc("truncate", path);
       if (!trunc.path) return -1;
       XrdPosixGlobals::theCache->Truncate(trunc.path, tSize);
      }

// Issue the truncate to the origin
//
   std::string urlp = admin.Url.GetPathWithParams();
   return XrdPosixMap::Result(admin.Xrd.Truncate(urlp,tSize));
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

// Unlink the cache first
//
   if (XrdPosixGlobals::theCache)
      {LfnPath remf("unlink", path);
       if (!remf.path) return -1;
       XrdPosixGlobals::theCache->Unlink(remf.path);
      }

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
   if (bytes < 0) return Fault(fp,-bytes);

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
/* Obsolete!                    s e t C a c h e                               */
/******************************************************************************/

void XrdPosixXrootd::setCache(XrdOucCache  *cP) {XrdPosixGlobals::myCache =cP;}

void XrdPosixXrootd::setCache(XrdOucCache2 *cP) {XrdPosixGlobals::myCache2=cP;}
  
/******************************************************************************/
/* Obsolete!                    s e t D e b u g                               */
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
/* Obsolete!                      s e t E n v                                 */
/******************************************************************************/

void XrdPosixXrootd::setEnv(const char *kword, int kval)
{
   XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
   static bool dlfSet = false;

// Check for internal envars before setting the external one
//
        if (!strcmp(kword, "DirlistAll"))
           {XrdPosixGlobals::dlFlag = (kval ? XrdCl::DirListFlags::Locate
                                            : XrdCl::DirListFlags::None);
            dlfSet = true;
           }
   else if (!strcmp(kword, "DirlistDflt"))
           {if (!dlfSet)
            XrdPosixGlobals::dlFlag = (kval ? XrdCl::DirListFlags::Locate
                                            : XrdCl::DirListFlags::None);
           }
   else env->PutInt((std::string)kword, kval);
}
  
/******************************************************************************/
/* Obsolete!                     s e t I P V 4                                */
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
/* Obsolete!                   s e t L o g g e r                              */
/******************************************************************************/

void XrdPosixXrootd::setLogger(XrdSysLogger *logP)
{
    XrdPosixGlobals::Trace.SetLogger(logP);
}
  
/******************************************************************************/
/* Obsolete!                    s e t N u m C B                               */
/******************************************************************************/

void XrdPosixXrootd::setNumCB(int numcb)
{
    if (numcb >= 0) XrdPosixFileRH::SetMax(numcb);
}
  
/******************************************************************************/
/* Obsolete!                      S e t N 2 N                                 */
/******************************************************************************/

void XrdPosixXrootd::setN2N(XrdOucName2Name *pN2N, int opts)
{
    XrdPosixGlobals::theN2N = pN2N;
}
  
/******************************************************************************/
/* Obsolete!                    s e t S c h e d                               */
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
