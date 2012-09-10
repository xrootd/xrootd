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

#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientAdmin.hh"
#include "XrdClient/XrdClientCallback.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdClient/XrdClientVector.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucCacheDram.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdPosix/XrdPosixCallBack.hh"
#include "XrdPosix/XrdPosixXrootd.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                X r d P o s i x A d m i n N e w   C l a s s                 */
/******************************************************************************/
  
class XrdPosixAdminNew
{
public:

XrdClientAdmin Admin;

int            Fault();

int            isOK() {return eNum == 0;}

int            lastError()
                   {return XrdPosixXrootd::mapError(Admin.LastServerError()->errnum);}

int            Result() {if (eNum) {errno = eNum; return -1;}
                         return 0;
                        }

      XrdPosixAdminNew(const char *path);
     ~XrdPosixAdminNew() {}

private:

int            eNum;
};

/******************************************************************************/
/*                     X r d P o s i x D i r   C l a s s                      */
/******************************************************************************/

class XrdPosixDir {

public:
  XrdPosixDir(int dirno, const char *path);
 ~XrdPosixDir();

  void             Lock()   { myMutex.Lock(); }
  void             UnLock() { myMutex.UnLock(); }
  int              dirNo()  { return fdirno; }
  long             getEntries() { return fentries.GetSize(); }
  long             getOffset() { return fentry; }
  void             setOffset(long offset) { fentry = offset; }
  dirent64        *nextEntry(dirent64 *dp=0);
  void             rewind() { fentry = -1; fentries.Clear();}
  int              Status() { return eNum;}

  
private:
  XrdSysMutex     myMutex;
  XrdClientAdmin  XAdmin;
  dirent64       *myDirent;
  static int      maxname;
  int             fdirno;
  char           *fpath;
  vecString       fentries;
  long            fentry;
  int             eNum;
};
  
/******************************************************************************/
/*                    X r d P o s i x F i l e   C l a s s                     */
/******************************************************************************/
  
class XrdPosixFile : public XrdOucCacheIO, public XrdClientCallback
{
public:

XrdOucCacheIO *XCio;
XrdClient     *XClient;

int        Active() {return doClose;}

void       isOpen();

long long  Offset() {return currOffset;}

long long  addOffset(long long offs, int updtSz=0)
                    {currOffset += offs;
                     if (updtSz && currOffset > stat.size) stat.size=currOffset;
                     return currOffset;
                    }

long long   FSize() {return stat.size;}

const char *Path();

int         Read (char *Buff, long long Offs, int Len)
                {return XClient->Read (Buff, Offs, Len);}

ssize_t     ReadV (const XrdSfsReadV *readV, size_t n)
{
   size_t nbytes;
   std::vector<long long> offsets; offsets.reserve(n);
   std::vector<int> lens; lens.reserve(n);
   for (int i=0; i<n; i++)
      {nbytes += readV[i].size;
       offsets[i] = readV[i].offset;
       lens[i] = readV[i].size;
      }
   std::vector<char> result_buffer;
   result_buffer.reserve(nbytes);
   ssize_t retval = XClient->ReadV(&(result_buffer[0]), &(offsets[0]), &(lens[0]), n);
   if (retval < 0)
       return retval;
   nbytes = 0;
   for (int i=0; i<n; i++)
      {memcpy(readV[i].data, &(result_buffer[nbytes]), readV[i].size);
       nbytes += readV[i].size;
      }
   return nbytes;
}

int         Sync() {return (XClient->Sync() ? 0 : -1);}

int         Trunc(long long Offset)
                 {return (XClient->Truncate(Offset) ? 0 : -1);}

int         Write(char *Buff, long long Offs, int Len)
                {return XClient->Write(Buff, Offs, Len);}

long long  setOffset(long long offs)
                    {currOffset = offs;
                     return currOffset;
                    }

void         Lock() {myMutex.Lock();}
void       UnLock() {myMutex.UnLock();}

void       OpenComplete(XrdClientAbs *clientP, void *cbArg, bool res)
                       {if (cbDone) return;
                        XrdPosixXrootd::OpenCB(this, cbArg, res);
                        cbDone=1;
                       }

XrdClientStatInfo stat;
XrdPosixCallBack *theCB;
XrdPosixFile     *Next;
char             *fPath;
int               FD;
int               cbResult;

static XrdOucCache *CacheR;
static XrdOucCache *CacheW;
static char        *sfSFX;
static int          sfSLN;

static const int realFD = 1;
static const int isSync = 2;

           XrdPosixFile(int fd, const char *path, int oMode,
                        XrdPosixCallBack *cbP=0, int Opts=realFD);
          ~XrdPosixFile();

private:

XrdSysMutex myMutex;
long long   currOffset;
int         cOpt;
char        doClose;
char        cbDone;
char        fdClose;
};


typedef XrdClientVector<XrdOucString> vecString;
typedef XrdClientVector<bool> vecBool;

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/
  
#define Scuttle(fp, rc) fp->UnLock(); errno = rc; return -1

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

XrdOucCache   *XrdPosixFile::CacheR   =  0;
XrdOucCache   *XrdPosixFile::CacheW   =  0;
char          *XrdPosixFile::sfSFX    =  0;
int            XrdPosixFile::sfSLN    =  0;

int            XrdPosixDir::maxname   = 255;

XrdSysMutex    XrdPosixXrootd::myMutex;
XrdPosixFile **XrdPosixXrootd::myFiles  =  0;
XrdPosixDir  **XrdPosixXrootd::myDirs   =  0;
XrdOucCache   *XrdPosixXrootd::myCache  =  0;
int            XrdPosixXrootd::highFD   = -1;
int            XrdPosixXrootd::lastFD   = -1;
int            XrdPosixXrootd::baseFD   =  0;
int            XrdPosixXrootd::freeFD   =  0;
int            XrdPosixXrootd::highDir  = -1;
int            XrdPosixXrootd::lastDir  = -1;
int            XrdPosixXrootd::devNull  = -1;
int            XrdPosixXrootd::pllOpen  =  0;
int            XrdPosixXrootd::maxThreads= 0;
int            XrdPosixXrootd::initDone  = 0;
int            XrdPosixXrootd::Debug    = -2;
  
/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/

void *XrdPosixXrootdCB(void *carg)
     {XrdPosixXrootd::OpenCB(0,0,0); return (void *)0;}
  
/******************************************************************************/
/*                X r d P o s i x A d m i n N e w   C l a s s                 */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdPosixAdminNew::XrdPosixAdminNew(const char *path) : Admin(path)
{

// Now Connect to the target host
//
   if (!Admin.Connect())
       eNum = XrdPosixXrootd::mapError(Admin.LastServerError()->errnum);
       else eNum = 0;
}

/******************************************************************************/
/*                                 F a u l t                                  */
/******************************************************************************/

int XrdPosixAdminNew::Fault()
{
   char *etext = Admin.LastServerError()->errmsg;
   int RC = XrdPosixXrootd::mapError(Admin.LastServerError()->errnum);

   if (RC != ENOENT && *etext && XrdPosixXrootd::Debug > -2)
      cerr <<"XrdPosix: " <<etext <<endl;
   errno = RC;
   return -1;
}
  
/******************************************************************************/
/*                           X r d P o s i x D i r                            */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixDir::XrdPosixDir(int dirno, const char *path) : XAdmin(path)
{

// Now connect up
//
   if (!XAdmin.Connect())
      eNum = XrdPosixXrootd::mapError(XAdmin.LastServerError()->errnum);
      else eNum = 0;

   fentry = -1;     // indicates that the directory content is not valid
   fentries.Clear();
   fdirno = dirno;

// Get the path of the url 
//
   XrdOucString str(path);
   XrdClientUrlInfo url(str);
   XrdOucString dir = url.File;
   fpath = strdup(dir.c_str());

// Allocate a local dirent. Note that we get additional padding because on
// some system the dirent structure does not include the name buffer
// Attempt to read the directory now so that we can reflect ENOENT at opendir()
//
   if ((myDirent = (dirent64 *)malloc(sizeof(dirent64) + maxname + 1)))
      {if (XAdmin.DirList(fpath,fentries)) fentry = 0;
          else eNum = XrdPosixXrootd::mapError(XAdmin.LastServerError()->errnum);
      } else eNum = ENOMEM;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdPosixDir::~XrdPosixDir()
{
  if (fpath)    free(fpath);
  if (myDirent) free(myDirent);
  close(fdirno);
}

/******************************************************************************/
/*                            n e x t E n t r y                               */
/******************************************************************************/

dirent64 *XrdPosixDir::nextEntry(dirent64 *dp)
{
   const char *cp;
   const int dirhdrln = dp->d_name - (char *)dp;
   int reclen;

// Object is already / must be locked at this point
// Read directory if we haven't done that yet
//
   if (fentry<0)
      {if (XAdmin.DirList(fpath,fentries)) fentry = 0;
          else {eNum = XrdPosixXrootd::mapError(XAdmin.LastServerError()->errnum);
                return 0;
               }
      }

// Check if dir is empty or all entries have been read
//
   if ((fentries.GetSize()==0) || (fentry>=fentries.GetSize())) return 0;

// Create a directory entry
//
   if (!dp) dp = myDirent;
   cp = (fentries[fentry]).c_str();
   reclen = strlen(cp);
   if (reclen > maxname) reclen = maxname;
#if defined(__macos__) || defined(__FreeBSD__)
   dp->d_fileno = fentry;
   dp->d_type   = DT_UNKNOWN;
   dp->d_namlen = reclen;
#else
   dp->d_ino    = fentry;
   dp->d_off    = fentry*maxname;
#endif
   dp->d_reclen = reclen + dirhdrln;
   strncpy(dp->d_name, cp, reclen);
   dp->d_name[reclen] = '\0';
   fentry++;
   return dp;
}

/******************************************************************************/
/*                          X r d P o s i x F i l e                           */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixFile::XrdPosixFile(int fd, const char *path, int oMode,
                           XrdPosixCallBack *cbP, int Opts)
             : XCio((XrdOucCacheIO *)this),
               theCB(cbP),
               Next(0),
               fPath(0),
               FD(fd),
               cbResult(0),
               currOffset(0),
               cOpt(0),
               doClose(0),
               cbDone(0),
               fdClose((Opts & realFD) != 0)
{
   static int OneVal = 1;
   XrdClientCallback *myCB = (cbP ? (XrdClientCallback *)this : 0);
   void *cbArg = (Opts & isSync ? (void *)&OneVal : 0);

// Allocate a new client object
//
   stat.size = 0;
   if (!(XClient = new XrdClient(path, myCB, cbArg))) return;

// Check for structured file check
//
   if (sfSFX)
      {int n = strlen(path);
       if (n > sfSLN && !strcmp(sfSFX, path + n - sfSLN))
          cOpt = XrdOucCache::optFIS;
      }

// Set cache options
//
   if (oMode & kXR_open_updt) cOpt |= XrdOucCache::optRW;
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdPosixFile::~XrdPosixFile()
{
   XrdClient *cP;
   if ((cP = XClient))
      {if (XCio) XCio = XCio->Detach();
       XClient = 0;
       if (doClose) {doClose = 0; cP->Close();}
       delete cP;
      }
   if (FD >= 0 && fdClose) close(FD);
   if (fPath) free(fPath);
}

/******************************************************************************/
/*                                i s O p e n                                 */
/******************************************************************************/
  
void XrdPosixFile::isOpen()
{

// See if we need to use a cache
//
   if (cOpt & XrdOucCache::optRW)
      {    if (CacheW) XCio = CacheW->Attach((XrdOucCacheIO *)this, cOpt);}
      else if (CacheR) XCio = CacheR->Attach((XrdOucCacheIO *)this, cOpt);

// Indicate file needs to be closed
//
   doClose = 1;
}

/******************************************************************************/
/*                                  P a t h                                   */
/******************************************************************************/
  
const char *XrdPosixFile::Path()
{
   if (!fPath) fPath = strdup(XClient->GetCurrentUrl().GetUrl().c_str());
   return fPath;
}

/******************************************************************************/
/*                         X r d P o s i x X r o o t d                        */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixXrootd::XrdPosixXrootd(int fdnum, int dirnum, int thrnum)
{
   struct rlimit rlim;
   long isize;

// Only static fields are initialized here. We need to do this only once!
//                                                                             c
   myMutex.Lock();
   if (initDone) {myMutex.UnLock(); return;}
   initDone = 1;
   myMutex.UnLock();

// Initialize environment if not done before. To avoid static initialization
// dependencies, we need to do it once but we must be the last ones to do it
// before any XrdClient library routines are called.
//
   initEnv();
   maxThreads = thrnum;

// Compute size of table. if the passed fdnum is negative then the caller does
// not want us to shadow fd's (ther caller promises to be honest). Otherwise,
// the actual fdnum limit will be based on the current limit.
//
   if (fdnum < 0)
      {fdnum = -fdnum;
       baseFD = ( getrlimit(RLIMIT_NOFILE, &rlim) ? 32768 : (int)rlim.rlim_cur);
      } else {
       if (!getrlimit(RLIMIT_NOFILE, &rlim))  fdnum = (int)rlim.rlim_cur;
       if (fdnum > 65536) fdnum = 65536;
      }
   isize = fdnum * sizeof(XrdPosixFile *);

// Allocate the table for fd-type pointers
//
   if (!(myFiles = (XrdPosixFile **)malloc(isize))) lastFD = -1;
      else {memset((void *)myFiles, 0, isize); lastFD = fdnum+baseFD;}

// Allocate table for DIR descriptors
//
   if (dirnum > 32768) dirnum = 32768;
   isize = dirnum * sizeof(XrdPosixDir *);
   if (!(myDirs = (XrdPosixDir **)malloc(isize))) lastDir = -1;
      else {memset((void *)myDirs, 0, isize); lastDir = dirnum;}

// Open /dev/null as we will be allocating file descriptors based on this fd
//
   devNull = open("/dev/null", O_RDWR, 0744);
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdPosixXrootd::~XrdPosixXrootd()
{
   XrdPosixDir  *dP;
   XrdPosixFile *fP;
   int i;

   myMutex.Lock();
   if (myFiles)
      {for (i = 0; i <= highFD; i++) 
           if ((fP = myFiles[i])) {myFiles[i] = 0; delete fP;};
       free(myFiles); myFiles = 0;
      }

   if (myDirs) {
     for (i = 0; i <= highDir; i++)
       if ((dP = myDirs[i])) {myDirs[i] = 0; delete dP;}
     free(myDirs); myDirs = 0;
   }

   initDone = 0;
   myMutex.UnLock();
}
 
/******************************************************************************/
/*                                A c c e s s                                 */
/******************************************************************************/
  
int XrdPosixXrootd::Access(const char *path, int amode)
{
   XrdPosixAdminNew admin(path);
   long st_flags, st_modtime, st_id;
   long long st_size;
   int st_mode, aOK = 1;

// Make sure we connected
//
   if (!admin.isOK()) return admin.Result();

// Extract out path from the url
//
   XrdOucString str(path);
   XrdClientUrlInfo url(str);

// Open the file first
//
   if (!admin.Admin.Stat(url.File.c_str(),
                         st_id, st_size, st_flags, st_modtime))
      {errno = admin.lastError();
       return -1;
      }

// Translate the mode bits
//
   st_mode = mapFlags(st_flags);
   if (amode & R_OK && !(st_mode & S_IRUSR)) aOK = 0;
   if (amode & W_OK && !(st_mode & S_IWUSR)) aOK = 0;
   if (amode & X_OK && !(st_mode & S_IXUSR)) aOK = 0;

// All done
//
   if (aOK) return 0;
   errno = EACCES;
   return -1;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

int     XrdPosixXrootd::Close(int fildes, int Stream)
{
   XrdPosixFile *fp;

// Find the file object. We tell findFP() to leave the global lock on
//
   if (!(fp = findFP(fildes, 1))) return -1;
   if (baseFD)
      {int myFD = fp->FD - baseFD;
       if (myFD < freeFD) freeFD = myFD;
       myFiles[myFD] = 0;
      } else myFiles[fp->FD] = 0;
   if (Stream) fp->FD = -1;

// Deallocate the file. We have the global lock.
//
   fp->UnLock();
   myMutex.UnLock();
   delete fp;
   return 0;
}

/******************************************************************************/
/*                              C l o s e d i r                               */
/******************************************************************************/

int XrdPosixXrootd::Closedir(DIR *dirp)
{
   XrdPosixDir *XrdDirp = findDIR(dirp,1);
   if (!XrdDirp) return -1;

// Deallocate the directory
//
   myDirs[XrdDirp->dirNo()] = 0;
   XrdDirp->UnLock();
   myMutex.UnLock();
   if (XrdDirp) delete XrdDirp;
   return 0;
}

/******************************************************************************/
/*                              e n d P o i n t                               */
/******************************************************************************/
  
int XrdPosixXrootd::endPoint(int FD, char *Buff, int Blen)
{
   XrdPosixFile    *fp;
   XrdClientUrlInfo fURL;

// Find the file object
//
   if (!(fp = findFP(FD))) return 0;

// Obtain the current url from the file
//
   fURL = fp->XClient->GetCurrentUrl();
   fp->UnLock();

// Make sure url is valid
//
   if (!fURL.IsValid()) return -ENOTCONN;

// Format host and port number, check if result is too long
//
   if (snprintf(Buff, Blen, "%s:%d", fURL.Host.c_str(), fURL.Port) >= Blen)
      return -ENAMETOOLONG;

// All done
//
   return fURL.Port;
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

// Return what little we can
//
   initStat(buf);
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
   if (fp->XCio->Sync() < 0) return Fault(fp);
   fp->UnLock();
   return 0;
}
  
/******************************************************************************/
/*                             F t r u n c a t e                              */
/******************************************************************************/
  
int     XrdPosixXrootd::Ftruncate(int fildes, off_t offset)
{
   XrdPosixFile *fp;

// Find the file object
//
   if (!(fp = findFP(fildes))) return -1;

// Do the sync
//
   if (fp->XCio->Trunc(offset) < 0) return Fault(fp);
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
  XrdPosixAdminNew admin(path);
  kXR_int16 ReqCode = 0;
  kXR_int32 vsize;
  time_t    mTm;

// Check if user just wants the maximum length needed
//
   if (size == 0) return 1024;
   vsize = static_cast<kXR_int32>(size);

// Check if we support the query
//
   if (name)
      {     if (!strcmp(name, "xroot.cksum"))
               return QueryChksum(path,mTm,(char *)value,static_cast<int>(size));
       else if (!strcmp(name, "xroot.space")) ReqCode = kXR_Qspace;
       else if (!strcmp(name, "xroot.xattr")) ReqCode = kXR_Qxattr;
       else {errno = ENOATTR; return -1;}
      }else {errno = EINVAL;  return -1;}

  if (admin.isOK())
     {XrdOucString str(path);
      XrdClientUrlInfo url(str);
      if (admin.Admin.Query(ReqCode, (kXR_char *)url.File.c_str(),
                                     (kXR_char *)value, vsize))
         return strlen((char *)value);
      return admin.Fault();
     }
  return admin.Result();
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
/*                                 M k d i r                                  */
/******************************************************************************/

int XrdPosixXrootd::Mkdir(const char *path, mode_t mode)
{
  XrdPosixAdminNew admin(path);
  int uMode = 0, gMode = 0, oMode = 0;

  if (admin.isOK())
     {XrdOucString str(path);
      XrdClientUrlInfo url(str);
      if (mode & S_IRUSR) uMode |= 4;
      if (mode & S_IWUSR) uMode |= 2;
      if (mode & S_IXUSR) uMode |= 1;
      if (mode & S_IRGRP) gMode |= 4;
      if (mode & S_IWGRP) gMode |= 2;
      if (mode & S_IXGRP) gMode |= 1;
      if (mode & S_IROTH) oMode |= 4;
      if (mode & S_IXOTH) oMode |= 1;
      if (admin.Admin.Mkdir(url.File.c_str(), uMode, gMode, oMode))
         return 0;
      return admin.Fault();
     }
  return admin.Result();
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdPosixXrootd::Open(const char *path, int oflags, mode_t mode,
                         XrdPosixCallBack *cbP)
{
   XrdPosixFile *fp;
   int Opts  = ((maxThreads==0) || (oflags & O_SYNC) ? XrdPosixFile::isSync : 0)
             | (baseFD ? 0 : XrdPosixFile::realFD);
   int XMode = (mode && (oflags & O_CREAT) ? mapMode(mode) : 0);
   int doclose = 1, retc = 0, fd, XOflags;

// Translate option bits to the appropraite values. Always 
// make directory path for new file.
//
   XOflags = (oflags & (O_WRONLY | O_RDWR) ? kXR_open_updt : kXR_open_read);
   if (oflags & O_CREAT) {
       XOflags |= (oflags & O_EXCL ? kXR_new : kXR_delete);
       XOflags |= kXR_mkpath;
   }
   else if (oflags & O_TRUNC && XOflags & kXR_open_updt)
              XOflags |= kXR_delete;

// Obtain a new filedscriptor from the system. Use the fd to track the file.
//
   if (baseFD)
      {myMutex.Lock();
       for (fd = freeFD; fd < baseFD && myFiles[fd]; fd++);
       if (fd >= baseFD || oflags & isStream) fd = lastFD;
          else freeFD = fd+1;
       doclose = 0;
      } else
        do{if ((fd = dup(devNull)) < 0) return -1;
           if (oflags & isStream && fd > 255)
              {close(fd); errno = EMFILE; return -1;}
           myMutex.Lock();
           if (fd >= lastFD || !myFiles[fd]) break;
           cerr <<"XrdPosix: FD " <<fd <<" closed outside of XrdPosix!" <<endl;
           myMutex.UnLock();
          } while(1);

// Allocate the new file object
//
   if (fd >= lastFD 
   ||  !(fp = new XrdPosixFile(fd+baseFD, path, XOflags, cbP, Opts)))
      {myMutex.UnLock();
       if (doclose) close(fd);
       errno = EMFILE;
       return -1;
      }
   myFiles[fd] = fp;
   if (fd > highFD) highFD = fd;
   myMutex.UnLock();

// Open the file
//
   if (!fp->XClient->Open(XMode, XOflags, (cbP ? 1 : pllOpen))
   || (!cbP && (fp->XClient->LastServerResp()->status != kXR_ok)))
      {retc = Fault(fp, 0);
       myMutex.Lock();
       myFiles[fd] = 0;
       delete fp;
       if (baseFD && fd < freeFD) freeFD = fd;
       if (doclose) close(fd);
       myMutex.UnLock();
       errno = retc;
       return -1;
      }

// If this is a callback open then just return EINPROGRESS
//
   if (cbP)
      {errno = EINPROGRESS;
       return -1;
      }

// Get the file size
//
   fp->XClient->Stat(&fp->stat);
   fp->isOpen();

// Return the fd number
//
   return fd + baseFD;
}

/******************************************************************************/
/*                                O p e n C B                                 */
/******************************************************************************/
  
void XrdPosixXrootd::OpenCB(XrdPosixFile *fp, void *cbArg, int res)
{
   static XrdSysMutex     cbMutex;
   static XrdSysSemaphore cbReady(0);
   static XrdPosixFile   *First = 0, *Last = 0;
   static int Waiting = 0, numThreads = 0;
          XrdPosixFile *cbFP;
          int rc = 0;

// If this is a feeder thread, look for some work
//
   if (!fp)
   do {cbMutex.Lock();
       if (!First && !Waiting)
          {numThreads--; cbMutex.UnLock(); return;}
       while(!(cbFP = First))
            {Waiting = 1;
             cbMutex.UnLock(); cbReady.Wait(); cbMutex.Lock();
             Waiting = 0;
            }
       if (!(First = cbFP->Next)) Last = 0;
       cbMutex.UnLock();
       if ((rc = (cbFP->cbResult < 0))) errno = -(cbFP->cbResult);
       cbFP->theCB->Complete(cbFP->cbResult);
       if (rc) delete cbFP;
      } while(1);

// Determine final result for this callback
//
   if (!res || (fp->XClient->LastServerResp()->status) != kXR_ok)
      {fp->cbResult = -Fault(fp, 0);
       myMutex.Lock();
       myFiles[fp->FD - baseFD] = 0;
       myMutex.UnLock();
      } else {
       fp->XClient->Stat(&fp->stat);
       fp->isOpen();
       fp->cbResult = fp->FD;
      }

// Lock our data structure and queue this element
//
   cbMutex.Lock();
   if (Last) Last->Next = fp;
      else   First    = fp;
   Last = fp; fp->Next = 0;

// See if we should start a thread
//
   if (!Waiting && numThreads < maxThreads)
      {pthread_t tid;
       if ((rc = XrdSysThread::Run(&tid, XrdPosixXrootdCB, (void *)0,
                                  0, "Callback thread")))
          cerr <<"XrdPosix: Unable to create callback thread; "
               <<strerror(rc) <<endl;
          else numThreads++;
      }

// All done
//
   cbReady.Post();
   cbMutex.UnLock();
}

/******************************************************************************/
/*                               O p e n d i r                                */
/******************************************************************************/
  
DIR* XrdPosixXrootd::Opendir(const char *path)
{
   XrdPosixDir *dirp = 0; // Avoid MacOS compiler warning
   int rc, fd;

// Obtain a new filedscriptor from the system. Use the fd to track the dir.
//
   if ((fd = dup(devNull)) < 0) return (DIR*)0;

// Allocate a new directory structure
//
   myMutex.Lock();
   if (fd > lastDir) rc = EMFILE;
      else if (!(dirp = new XrdPosixDir(fd, path))) rc = ENOMEM;
              else rc = dirp->Status();

   if (!rc)
      {myDirs[fd] = dirp;
       if (fd > highDir) highDir = fd;
      }
   myMutex.UnLock();

   if (rc) {if (dirp) {delete dirp; dirp = 0;}
               else close(fd);
            errno = rc;
           }

   return (DIR*)dirp;
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
   bytes = fp->XCio->Read((char *)buf, offs, (int)iosz);
   if (bytes <= 0) return Fault(fp,-1);

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
   if (!(fp = findFP(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) {Scuttle(fp,EOVERFLOW);}
      else iosz = static_cast<int>(nbyte);

// Issue the write
//
   offs = static_cast<long long>(offset);
   bytes = fp->XCio->Write((char *)buf, offs, (int)iosz);
   if (bytes <= 0 && iosz) return Fault(fp);

// All went well
//
   if (offs+iosz > fp->stat.size) fp->stat.size = offs + iosz;
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
   if (!(fp = findFP(fildes))) return -1;


// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) {Scuttle(fp,EOVERFLOW);}
      else iosz = static_cast<int>(nbyte);

// Issue the read
//
   bytes = fp->XCio->Read((char *)buf,fp->Offset(),(int)iosz);
   if (bytes <= 0) return Fault(fp,-1);

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
/*                                 R e a d v 2                                */
/******************************************************************************/

ssize_t XrdPosixXrootd::Readv2(int fildes, const XrdSfsReadV *readV, int n)
{
   XrdPosixFile *fp;
   int           iosz;

// Find the file object
//
   if (!(fp = findFP(fildes))) return -1;

// Issue the read
//
   ssize_t bytes = fp->XCio->ReadV(readV, n);
   if (bytes < 0) return Fault(fp,-1);

// All went well
//
   fp->UnLock();
   return (ssize_t)bytes;
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
   if (dp32->d_name != dp64->d_name)
      {dp32->d_ino    = dp64->d_ino;
#if !defined(__macos__) && !defined(__FreeBSD__)
       dp32->d_off    = dp64->d_off;
#endif
       dp32->d_reclen = dp64->d_reclen;
       strcpy(dp32->d_name, dp64->d_name);
      }
   return dp32;
}

struct dirent64* XrdPosixXrootd::Readdir64(DIR *dirp)
{
   XrdPosixDir *XrdDirp = findDIR(dirp);
   dirent64 *dp;
   int rc;

// Returns the next directory entry
//
   if (!XrdDirp) return 0;

   if (!(dp = XrdDirp->nextEntry())) rc = XrdDirp->Status();
      else rc = 0;


   XrdDirp->UnLock();
   if (rc) errno = rc;
   return dp;
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
#if !defined(__macos__) && !defined(__FreeBSD__)
   entry->d_off    = dp64->d_off;
#endif
   entry->d_reclen = dp64->d_reclen;
   strcpy(entry->d_name, dp64->d_name);
   *result = entry;
   return rc;
}

int XrdPosixXrootd::Readdir64_r(DIR *dirp, struct dirent64  *entry,
                                           struct dirent64 **result)
{
   XrdPosixDir *XrdDirp = findDIR(dirp);
   int rc;

// Thread safe verison of readdir
//
   if (!XrdDirp) {errno = EBADF; return -1;}

   if (!(*result = XrdDirp->nextEntry(entry))) rc = XrdDirp->Status();
      else rc = 0;

   XrdDirp->UnLock();
   return rc;
}

/******************************************************************************/
/*                                R e n a m e                                 */
/******************************************************************************/

int XrdPosixXrootd::Rename(const char *oldpath, const char *newpath)
{
  XrdPosixAdminNew admin(oldpath);

  if (admin.isOK())
     {XrdOucString    oldstr(oldpath);
      XrdClientUrlInfo oldurl(oldstr);
      XrdOucString    newstr(newpath);
      XrdClientUrlInfo newurl(newstr);
      if (admin.Admin.Mv(oldurl.File.c_str(),
                         newurl.File.c_str())) return 0;
      return admin.Fault();
     }
  return admin.Result();
}


/******************************************************************************/
/*                            R e w i n d d i r                               */
/******************************************************************************/

void XrdPosixXrootd::Rewinddir(DIR *dirp)
{
// Updates and rewinds directory
//
   XrdPosixDir *XrdDirp = findDIR(dirp);
   if (!XrdDirp) return;

   XrdDirp->rewind();
   XrdDirp->UnLock();
}

/******************************************************************************/
/*                                 R m d i r                                  */
/******************************************************************************/

int XrdPosixXrootd::Rmdir(const char *path)
{
  XrdPosixAdminNew admin(path);

  if (admin.isOK())
     {XrdOucString str(path);
      XrdClientUrlInfo url(str);
      if (admin.Admin.Rmdir(url.File.c_str())) return 0;
      return admin.Fault();
     }
  return admin.Result();
}

/******************************************************************************/
/*                                S e e k d i r                               */
/******************************************************************************/

void XrdPosixXrootd::Seekdir(DIR *dirp, long loc)
{
// Sets the current directory position
//
   XrdPosixDir *XrdDirp = findDIR(dirp);
   if (!XrdDirp) return;
   
   if (XrdDirp->getOffset()<0) XrdDirp->nextEntry();  // read the directory
   if (loc >= XrdDirp->getEntries()) loc = XrdDirp->getEntries()-1;
   else if (loc<0) loc = 0;

   XrdDirp->setOffset(loc);
   XrdDirp->UnLock();
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/
  
int XrdPosixXrootd::Stat(const char *path, struct stat *buf)
{
   XrdPosixAdminNew admin(path);
   long st_flags, st_modtime, st_id;
   long long st_size;

// Make sure we connected
//
   if (!admin.isOK()) return admin.Result();

// Extract out path from the url
//
   XrdOucString str(path);
   XrdClientUrlInfo url(str);

// Open the file first
//
   if (!admin.Admin.Stat(url.File.c_str(),
                         st_id, st_size, st_flags, st_modtime))
      return admin.Fault();

// Return what little we can
//
   initStat(buf);
   buf->st_size   = st_size;
   buf->st_blocks = buf->st_size/512+1;
   buf->st_atime  = buf->st_mtime = buf->st_ctime = st_modtime;
   buf->st_ino    = st_id;
   buf->st_mode   = mapFlags(st_flags);
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
#if defined(__macos__) || defined(__FreeBSD__)
   buf->f_iosize  = myVfs.f_frsize;
#else
   buf->f_frsize  = myVfs.f_frsize;
#endif
#if defined(__linux__) || defined(__macos__) || defined(__FreeBSD__)
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
   XrdPosixAdminNew admin(path);
   long long rwFree, ssFree, rwBlks;
   int       rwNum, ssNum, rwUtil, ssUtil;

// Make sure we connected
//
   if (!admin.isOK()) return admin.Result();

// Extract out path from the url
//
   XrdOucString str(path);
   XrdClientUrlInfo url(str);

// Open the file first
//
   if (!admin.Admin.Stat_vfs(url.File.c_str(),
       rwNum, rwFree, rwUtil, ssNum, ssFree, ssUtil)) return admin.Fault();
   if (rwNum < 0) {errno = ENOENT; return -1;}

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
// Tells the current directory location
//
   XrdPosixDir *XrdDirp = findDIR(dirp);
   if (!XrdDirp) return -1;

   long pos;
   if (XrdDirp->getOffset()<0) pos = 0;  // dir is open but not read yet
   else pos = XrdDirp->getOffset();
   XrdDirp->UnLock();
   return pos;
}

/******************************************************************************/
/*                              T r u n c a t e                               */
/******************************************************************************/
  
int XrdPosixXrootd::Truncate(const char *path, off_t Size)
{
  XrdPosixAdminNew admin(path);

  if (admin.isOK())
     {XrdOucString str(path);
      XrdClientUrlInfo url(str);
      if (admin.Admin.Truncate(url.File.c_str(), Size)) return 0;
      return admin.Fault();
     }
  return admin.Result();
}

/******************************************************************************/
/*                                 U n l i n k                                */
/******************************************************************************/

int XrdPosixXrootd::Unlink(const char *path)
{
  XrdPosixAdminNew admin(path);

  if (admin.isOK())
     {XrdOucString str(path);
      XrdClientUrlInfo url(str);
      if (admin.Admin.Rm(url.File.c_str())) return 0;
      return admin.Fault();
     }
  return admin.Result();
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
   if (!(fp = findFP(fildes))) return -1;

// Make sure the size is not too large
//
   if (nbyte > (size_t)0x7fffffff) {Scuttle(fp,EOVERFLOW);}
      else iosz = static_cast<int>(nbyte);

// Issue the write
//
   bytes = fp->XCio->Write((char *)buf,fp->Offset(),(int)iosz);
   if (bytes <= 0 && iosz) return Fault(fp);

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
   struct XrdPosix_Env {const char *eName; const char *xName; int *vDest;}
          Posix_Env[] =
          {
          {"XRDPOSIX_DEBUG",       NAME_DEBUG,                &Debug},
          {"XRDPOSIX_DSTTL",       NAME_DATASERVERCONN_TTL,   0},
          {"XRDPOSIX_POPEN",       "",                        &pllOpen},
          {"XRDPOSIX_RASZ",        NAME_READAHEADSIZE,        0},
          {"XRDPOSIX_RCSZ",        NAME_READCACHESIZE,        0},
          {"XRDPOSIX_RCRP",        NAME_READCACHEBLKREMPOLICY,0},
          {"XRDPOSIX_RCUP",        NAME_REMUSEDCACHEBLKS,     0},
          {"XRDPOSIX_RDTTL",       NAME_LBSERVERCONN_TTL,     0},
          {"XRDPOSIX_RTO",         NAME_REQUESTTIMEOUT,       0},
          {"XRDPOSIX_TTO",         NAME_TRANSACTIONTIMEOUT,   0},
          {"XRDPSOIX_PSPC",        NAME_MULTISTREAMCNT,       0},
          {"XRDPOSIX_CTO",         NAME_CONNECTTIMEOUT,       0},
          {"XRDPOSIX_CRDELAY",     NAME_RECONNECTWAIT,        0},
          {"XRDPOSIX_CRETRY",      NAME_FIRSTCONNECTMAXCNT,   0},
          {"XRDPOSIX_TCPWSZ",      NAME_DFLTTCPWINDOWSIZE,    0}
          };
   int    Posix_Num = sizeof(Posix_Env)/sizeof(XrdPosix_Env);
   char *cvar, *evar;
   int i, doEcho;
   long nval;

// Establish wether we need to echo any envars
//
   if ((cvar = getenv("XRDPOSIX_ECHO"))) doEcho = strcmp(cvar, "0");
      else doEcho = 0;

// Establish the default debugging level (none)
//
   setEnv(NAME_DEBUG, Debug);

// Run through all of the numeric envars that may be set
//
   for (i = 0; i < Posix_Num; i++)
       if ((cvar = getenv(Posix_Env[i].eName)) && *cvar)
          {nval = strtol(cvar, &evar, 10);
           if (*evar) cerr <<"XrdPosix: Invalid " <<Posix_Env[i].eName
                           <<" value - " <<cvar <<endl;
              else {if (Posix_Env[i].xName[0]) setEnv(Posix_Env[i].xName, nval);
                    if (Posix_Env[i].vDest)
                       *Posix_Env[i].vDest = static_cast<int>(nval);
                    if (doEcho) cerr <<"XrdPosix: " <<Posix_Env[i].eName <<" = "
                                <<nval <<'(' <<Posix_Env[i].xName <<')' <<endl;
                   }
          }

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
      else {setEnv(NAME_READAHEADSIZE, (long)0);
            setEnv(NAME_READCACHESIZE, (long)0);
            if (isRW) XrdPosixFile::CacheW = XrdPosixFile::CacheR;
           }
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
   if (!dirp) return false;

   for (int i = 0; i <= highDir; i++) 
     if (((XrdPosixDir*)dirp)==myDirs[i]) return true;

   return false;
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
        case kXR_isDirectory:   return EISDIR;
        case kXR_FSError:       return ENOSYS;
        default:                return ECANCELED;
       }
}

/******************************************************************************/
/*                           Q u e r y C h k s u m                            */
/******************************************************************************/
  
int XrdPosixXrootd::QueryChksum(const char *path,  time_t &Mtime,
                                      char *value, int     vsize)
{
   XrdPosixAdminNew admin(path);
   long long st_size;
   long st_flags, st_modtime, st_id;

// Make sure we connected
//
   if (!admin.isOK()) return admin.Result();

// Extract out path from the url
//
   XrdOucString str(path);
   XrdClientUrlInfo url(str);

// Stat the file first to allow vectoring of the request to the right server
//
   if (!admin.Admin.Stat(url.File.c_str(),st_id,st_size,st_flags,st_modtime))
      return admin.Fault();

// Now we can get the checksum as we have landed on the right server
//
   if (!admin.Admin.Query(kXR_Qcksum, (kXR_char *)url.File.c_str(),
                                      (kXR_char *)value, vsize))
      return admin.Result();

// Return results
//
   Mtime = static_cast<time_t>(st_modtime);
   return strlen((char *)value);
}
  
/******************************************************************************/
/*                           Q u e r y O p a q u e                            */
/******************************************************************************/
  
long long XrdPosixXrootd::QueryOpaque(const char *path, char *value, int size)
{
  XrdPosixAdminNew admin(path);
  kXR_int32 vsize = static_cast<kXR_int32>(size);

  if (admin.isOK())
     {XrdOucString str(path);
      XrdClientUrlInfo url(str);
      admin.Admin.GoBackToRedirector();
      if (admin.Admin.Query(kXR_Qopaquf, (kXR_char *)url.File.c_str(), 
                                         (kXR_char *)value, vsize))
         return strlen((char *)value);
      return admin.Fault();
     }
  return admin.Result();
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

void XrdPosixXrootd::setDebug(int val)
{
     Debug = static_cast<long>(val);
     setEnv("DebugLevel", val);
}
  
/******************************************************************************/
/*                                s e t E n v                                 */
/******************************************************************************/
  
void XrdPosixXrootd::setEnv(const char *var, const char *val)
{
     EnvPutString(var, val);
}

void XrdPosixXrootd::setEnv(const char *var, long val)
{
     EnvPutInt(var, val);
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 F a u l t                                  */
/******************************************************************************/

int XrdPosixXrootd::Fault(XrdPosixFile *fp, int complete)
{
   char *etext = fp->XClient->LastServerError()->errmsg;
   int   ecode = fp->XClient->LastServerError()->errnum;
   int   rc = -1;

   if (complete < 0)
      {if (ecode && ecode != kXR_noErrorYet) ecode = mapError(ecode);
          else ecode = rc = 0;
      } else {
       ecode = mapError(ecode);
       if (ecode != ENOENT && *etext && XrdPosixXrootd::Debug > -2)
          cerr <<"XrdPosix: " <<etext <<endl;
      }

   if (!complete) return ecode;
   fp->UnLock();
   errno = ecode;
   return rc;
}
  
/******************************************************************************/
/*                                f i n d F P                                 */
/******************************************************************************/
  
XrdPosixFile *XrdPosixXrootd::findFP(int fd, int glk)
{
   XrdPosixFile *fp;

// Validate the fildes
//
   if (fd >= lastFD || fd < baseFD)
      {errno = EBADF; return (XrdPosixFile *)0;}

// Obtain the file object, if any
//
   myMutex.Lock();
   if (!(fp = myFiles[fd - baseFD]) || !(fp->Active()))
      {myMutex.UnLock(); errno = EBADF; return (XrdPosixFile *)0;}

// Lock the object and unlock the global lock unless it is to be held
//
   fp->Lock();
   if (!glk) myMutex.UnLock();
   return fp;
}

/******************************************************************************/
/*                                f i n d D I R                               */
/******************************************************************************/

XrdPosixDir *XrdPosixXrootd::findDIR(DIR *dirp, int glk)
{
   if (!dirp) { errno = EBADF; return 0; }

// Check if this is really an open directory
//
   XrdPosixDir *XrdDirp = (XrdPosixDir*)dirp;
   myMutex.Lock();
   if (!(myDirs[XrdDirp->dirNo()]==XrdDirp))
      {myMutex.UnLock();
       errno = EBADF;
       return 0;
      }

// Lock the object and unlock the global lock unless it is to be held
//
   XrdDirp->Lock();
   if (!glk) myMutex.UnLock();
   return XrdDirp;
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
   struct stat buf;

// Get the device id for /tmp used by stat()
//
   if (stat("/tmp", &buf)) {st_dev = 0; st_rdev = 0;}
      else {st_dev = buf.st_dev; st_rdev = buf.st_rdev;}
}

/******************************************************************************/
/*                              m a p F l a g s                               */
/******************************************************************************/
  
int XrdPosixXrootd::mapFlags(int flags)
{
   int newflags = 0;

// Map the xroot flags to unix flags
//
   if (flags & kXR_xset)     newflags |= S_IXUSR;
   if (flags & kXR_readable) newflags |= S_IRUSR;
   if (flags & kXR_writable) newflags |= S_IWUSR;
   if (flags & kXR_other)    newflags |= S_IFBLK;
      else if (flags & kXR_isDir) newflags |= S_IFDIR;
              else newflags |= S_IFREG;
   if (flags & kXR_offline) newflags |= S_ISVTX;
   if (flags & kXR_poscpend)newflags |= S_ISUID;

   return newflags;
}

/******************************************************************************/
/*                               m a p M o d e                                */
/******************************************************************************/
  
int XrdPosixXrootd::mapMode(mode_t mode)
{  int XMode = 0;

// Map the mode
//
   if (mode & S_IRUSR) XMode |= kXR_ur;
   if (mode & S_IWUSR) XMode |= kXR_uw;
   if (mode & S_IXUSR) XMode |= kXR_ux;
   if (mode & S_IRGRP) XMode |= kXR_gr;
   if (mode & S_IWGRP) XMode |= kXR_gw;
   if (mode & S_IXGRP) XMode |= kXR_gx;
   if (mode & S_IROTH) XMode |= kXR_or;
   if (mode & S_IXOTH) XMode |= kXR_ox;
   return XMode;
}
