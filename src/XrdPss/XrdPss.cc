/******************************************************************************/
/*                                                                            */
/*                             X r d P s s . c c                              */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

/******************************************************************************/
/*                             I n c l u d e s                                */
/******************************************************************************/
  
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <strings.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __solaris__
#include <sys/vnode.h>
#endif

#include "XrdVersion.hh"

#include "XrdFfs/XrdFfsPosix.hh"
#include "XrdNet/XrdNetSecurity.hh"
#include "XrdPss/XrdPss.hh"
#include "XrdPosix/XrdPosixXrootd.hh"

#include "XrdOss/XrdOssError.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

#define isNOSTAGE(_x_) !(XRDEXP_STAGE & XrdPssSys::XPList.Find(_x_))

#define isREADONLY(_x_) (XRDEXP_NOTRW & XrdPssSys::XPList.Find(_x_))

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

class XrdScheduler;

namespace XrdProxy
{
static XrdPssSys   XrdProxySS;
  
       XrdSysError eDest(0, "proxy_");

static XrdScheduler *schedP = 0;

static const char *ofslclCGI = "ofs.lcl=1";
static const int   ofslclCGL = strlen(ofslclCGI);

static const char *osslclCGI = "oss.lcl=1";
static const int   osslclCGL = strlen(osslclCGI);

static const int   PBsz = 4096;

static const int   CBsz = 2048;
}

using namespace XrdProxy;

/******************************************************************************/
/*                XrdOssGetSS (a.k.a. XrdOssGetStorageSystem)                 */
/******************************************************************************/

XrdVERSIONINFO(XrdOssGetStorageSystem,XrdPss);
  
// This function is called by the OFS layer to retrieve the Storage System
// object. We return our proxy storage system object if configuration succeeded.
//
extern "C"
{
XrdOss *XrdOssGetStorageSystem(XrdOss       *native_oss,
                               XrdSysLogger *Logger,
                               const char   *config_fn,
                               const char   *parms)
{

// Ignore the parms (we accept none for now) and call the init routine
//
   return (XrdProxySS.Init(Logger, config_fn) ? 0 : (XrdOss *)&XrdProxySS);
}
}
 
/******************************************************************************/
/*                      o o s s _ S y s   M e t h o d s                       */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdPssSys::XrdPssSys() : LocalRoot(0), N2NLib(0), N2NParms(0), theN2N(0),
                         DirFlags(0), cPath(0), cParm(0),
                         myVersion(&XrdVERSIONINFOVAR(XrdOssGetStorageSystem)),
                         TraceLvl(0) {}

/******************************************************************************/
/*                                  i n i t                                   */
/******************************************************************************/
  
/*
  Function: Initialize proxy subsystem

  Input:    None

  Output:   Returns zero upon success otherwise (-errno).
*/
int XrdPssSys::Init(XrdSysLogger *lp, const char *configfn)
{
   int NoGo;
   const char *tmp;

// Do the herald thing
//
   eDest.logger(lp);
   eDest.Say("Copr.  2007, Stanford University, Pss Version " XrdVSTRING);

// Initialize the subsystems
//
   tmp = ((NoGo=Configure(configfn)) ? "failed." : "completed.");
   eDest.Say("------ Proxy storage system initialization ", tmp);

// All done.
//
   return NoGo;
}

/******************************************************************************/
/*                                 C h m o d                                  */
/******************************************************************************/
/*
  Function: Change file mode.

  Input:    path        - Is the fully qualified name of the target file.
            mode        - The new mode that the file is to have.
            envP        - Environmental information.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    This function is currently unsupported.
*/

int XrdPssSys::Chmod(const char *path, mode_t mode, XrdOucEnv *eP)
{
// We currently do not support chmod()
//
   return -ENOTSUP;
}

/******************************************************************************/
/*                                c r e a t e                                 */
/******************************************************************************/

/*
  Function: Create a file named `path' with 'file_mode' access mode bits set.

  Input:    path        - The fully qualified name of the file to create.
            access_mode - The Posix access mode bits to be assigned to the file.
                          These bits correspond to the standard Unix permission
                          bits (e.g., 744 == "rwxr--r--").
            env         - Environmental information.
            opts        - Set as follows:
                          XRDOSS_mkpath - create dir path if it does not exist.
                          XRDOSS_new    - the file must not already exist.
                          x00000000     - x are standard open flags (<<8)

  Output:   Returns XrdOssOK upon success; (-errno) otherwise.

  Notes:    We always return ENOTSUP as we really want the create options to be
            promoted to the subsequent open().
*/
int XrdPssSys::Create(const char *tident, const char *path, mode_t Mode,
                        XrdOucEnv &env, int Opts)
{

   return -ENOTSUP;
}

/******************************************************************************/
/*                               E n v I n f o                                */
/******************************************************************************/
  
void        XrdPssSys::EnvInfo(XrdOucEnv *envP)
{
// We only need to extract the scheduler pointer from the environment
//
   if (envP) schedP = (XrdScheduler *)envP->GetPtr("XrdScheduler*");

   XrdPosixXrootd::setSched(schedP);
}
  
/******************************************************************************/
/*                               L f n 2 P f n                                */
/******************************************************************************/
  
int         XrdPssSys::Lfn2Pfn(const char *oldp, char *newp, int blen)
{
    if (theN2N) return -(theN2N->lfn2pfn(oldp, newp, blen));
    if ((int)strlen(oldp) >= blen) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
}

const char *XrdPssSys::Lfn2Pfn(const char *oldp, char *newp, int blen, int &rc)
{
    if (!theN2N) {rc = 0; return oldp;}
    if ((rc = -(theN2N->lfn2pfn(oldp, newp, blen)))) return 0;
    return newp;
}

/******************************************************************************/
/*                                 M k d i r                                  */
/******************************************************************************/
/*
  Function: Create a directory

  Input:    path        - Is the fully qualified name of the new directory.
            mode        - The new mode that the directory is to have.
            mkpath      - If true, makes the full path.
            envP        - Environmental information.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    Directories are only created in the local disk cache.
            Currently, we do not propogate the mkpath option.
*/

int XrdPssSys::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *eP)
{
   int retc, CgiLen;
   const char *Cgi = eP->Env(CgiLen);
   char pbuff[PBsz];

// Verify we can write here
//
   if (isREADONLY(path)) return -EROFS;

// Convert path to URL
//
   if (!P2URL(retc, pbuff, PBsz, path, 0, Cgi, CgiLen)) return retc;

// Simply return the proxied result here
//
   return (XrdPosixXrootd::Mkdir(pbuff, mode) ? -errno : XrdOssOK);
}
  
/******************************************************************************/
/*                                R e m d i r                                 */
/******************************************************************************/

/*
  Function: Removes the directory 'path'

  Input:    path      - Is the fully qualified name of the directory to remove.
            envP      - Environmental information.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdPssSys::Remdir(const char *path, int Opts, XrdOucEnv *eP)
{
   int rc, CgiLen;
   const char *Cgi  = eP->Env(CgiLen);
   char pbuff[PBsz], cbuff[CBsz], *subPath;

// Verify we can write here
//
   if (isREADONLY(path)) return -EROFS;

// Setup any required cgi information
//
   if (*path == '/' && !outProxy && (Opts & XRDOSS_Online))
      {if (!Cgi) {Cgi = ofslclCGI; CgiLen = ofslclCGL;}
          else   {Cgi = P2CGI(CgiLen, cbuff, sizeof(cbuff), Cgi, ofslclCGI);
                  if (!Cgi) return -ENAMETOOLONG;
                 }
      }

// Convert path to URL
//
   if (!(subPath = P2URL(rc, pbuff, PBsz, path, allRmdir, Cgi, CgiLen)))
      return rc;

// If unlinks are being forwarded, just execute this on a single node.
// Otherwise, make sure it it's not the base dir and execute everywhere.
//
   if (!allRmdir || *path != '/') rc = XrdPosixXrootd::Rmdir(pbuff);
      else {if (!(*subPath)) return -EPERM;
            if (!cfgDone)    return -EBUSY;
            rc = XrdFfsPosix_rmdirall(pbuff, subPath, myUid);
           }

// Return the result
//
   return (rc ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                                R e n a m e                                 */
/******************************************************************************/

/*
  Function: Renames a file with name 'old_name' to 'new_name'.

  Input:    old_name  - Is the fully qualified name of the file to be renamed.
            new_name  - Is the fully qualified name that the file is to have.
            old_envP  - Environmental information for old_name.
            new_envP  - Environmental information for new_name.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdPssSys::Rename(const char *oldname, const char *newname,
                      XrdOucEnv  *oldenvP, XrdOucEnv  *newenvP)
{
   const char *oldCgi, *newCgi;
   char oldName[PBsz], *oldSubP, newName[PBsz], *newSubP;
   int retc, oldCgiLen, newCgiLen;

// Verify we can write in the source and target
//
   if (isREADONLY(oldname) || isREADONLY(newname)) return -EROFS;

// Grab any cgi information
//
   oldCgi = oldenvP->Env(oldCgiLen);
   newCgi = newenvP->Env(newCgiLen);

// If we are not forwarding the request, manually execute it everywhere.
//
   if (allMv && *oldname == '/')
      {if (!cfgDone) return -EBUSY;
       return (XrdFfsPosix_renameall(urlPlain, oldname, newname, myUid)
              ? -errno : XrdOssOK);
      }

// Convert path to URL
//
   if (!(oldSubP = P2URL(retc, oldName, PBsz, oldname, 0, oldCgi, oldCgiLen))
   ||  !(newSubP = P2URL(retc, newName, PBsz, newname, 0, newCgi, newCgiLen)))
       return retc;

// Execute the rename and return result
//
   return (XrdPosixXrootd::Rename(oldName, newName) ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                                 s t a t                                    */
/******************************************************************************/

/*
  Function: Determine if file 'path' actually exists.

  Input:    path        - Is the fully qualified name of the file to be tested.
            buff        - pointer to a 'stat' structure to hold the attributes
                          of the file.
            Opts        - stat() options.
            envP        - Environmental information.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    The XRDOSS_resonly flag in Opts is not supported.
*/

int XrdPssSys::Stat(const char *path, struct stat *buff, int Opts, XrdOucEnv *eP)
{
   int CgiLen, retc;
   const char *Cgi = (eP ? eP->Env(CgiLen) : 0);
   char pbuff[PBsz], cbuff[CBsz];

// Setup any required cgi information
//
   if (*path == '/' && !outProxy && ((Opts & XRDOSS_resonly)||isNOSTAGE(path)))
      {if (!Cgi) {Cgi = osslclCGI; CgiLen = osslclCGL;}
          else   {Cgi = P2CGI(CgiLen, cbuff, sizeof(cbuff), Cgi, osslclCGI);
                  if (!Cgi) return -ENAMETOOLONG;
                 }
      }

// Convert path to URL
//
   if (!P2URL(retc, pbuff, PBsz, path, 0, Cgi, CgiLen)) return retc;

// Return proxied stat
//
   return (XrdPosixXrootd::Stat(pbuff, buff) ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                              T r u n c a t e                               */
/******************************************************************************/
/*
  Function: Truncate a file.

  Input:    path        - Is the fully qualified name of the target file.
            flen        - The new size that the file is to have.
            envP        - Environmental information.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

int XrdPssSys::Truncate(const char *path, unsigned long long flen,
                        XrdOucEnv *envP)
{
   int CgiLen, retc;
   const char *Cgi = envP->Env(CgiLen);
   char pbuff[PBsz];

// Make sure we can write here
//
   if (isREADONLY(path)) return -EROFS;

// Convert path to URL
//
   if (!P2URL(retc, pbuff, PBsz, path, 0, Cgi, CgiLen)) return retc;

// Return proxied truncate. We only do this on a single machine because the
// redirector will forbid the trunc() if multiple copies exist.
//
   return (XrdPosixXrootd::Truncate(pbuff, flen) ? -errno : XrdOssOK);
}
  
/******************************************************************************/
/*                                U n l i n k                                 */
/******************************************************************************/

/*
  Function: Delete a file from the namespace and release it's data storage.

  Input:    path      - Is the fully qualified name of the file to be removed.
            envP      - Environmental information.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdPssSys::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
   int CgiLen, rc;
   const char *Cgi = envP->Env(CgiLen);
   char *subPath, pbuff[PBsz], cbuff[CBsz];

// Make sure we can write here
//
   if (isREADONLY(path)) return -EROFS;

// Setup any required cgi information
//
   if (*path == '/' && !outProxy && (Opts & XRDOSS_Online))
      {if (!Cgi) {Cgi = ofslclCGI; CgiLen = ofslclCGL;}
          else   {Cgi = P2CGI(CgiLen, cbuff, sizeof(cbuff), Cgi, ofslclCGI);
                  if (!Cgi) return -ENAMETOOLONG;
                 }
      }

// Convert path to URL
//
   if (!(subPath = P2URL(rc, pbuff, PBsz, path, allRm, Cgi, CgiLen)))
      return rc;

// If unlinks are being forwarded, just execute this on a single node.
// Otherwise, make sure it may be a file and execute everywhere.
//
   if (!allRm || *path != '/') rc = XrdPosixXrootd::Unlink(pbuff);
      else {if (!(*subPath)) return -EISDIR;
            if (!cfgDone)    return -EBUSY;
            rc = XrdFfsPosix_unlinkall(pbuff, subPath, myUid);
           }

// Return the result
//
   return (rc ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                        P s s D i r   M e t h o d s                         */
/******************************************************************************/
/******************************************************************************/
/*                               o p e n d i r                                */
/******************************************************************************/
  
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.
            envP      - Environmental information.

  Output:   Returns XrdOssOK upon success; (-errno) otherwise.
*/
int XrdPssDir::Opendir(const char *dir_path, XrdOucEnv &Env)
{
   int CgiLen, retc;
   const char *Cgi = Env.Env(CgiLen);
   char pbuff[PBsz], *subPath;
   int theUid = XrdPssSys::T2UID(tident);

// Return an error if this object is already open
//
   if (dirVec || myDir) return -XRDOSS_E8001;
   if (!XrdPssSys::outProxy && !XrdProxySS.cfgDone) return -EBUSY;

// Open directories are not supported for object id's
//
   if (*dir_path != '/') return -ENOTSUP;

// Convert path to URL
//
   if (!(subPath = XrdPssSys::P2URL(retc,pbuff,PBsz,dir_path,0,Cgi,CgiLen)))
      return retc;

// If we are an outgoing proxy then do the simple thing
//
   if (XrdPssSys::outProxy)
      {myDir = XrdPosixXrootd::Opendir(pbuff);
       if (!myDir) return -errno;
       return XrdOssOK;
      }

// Return proxied result
//
   if ((numEnt = XrdFfsPosix_readdirall(pbuff, "", &dirVec, theUid)) < 0)
       {int rc = -errno;
        if (dirVec) {free(dirVec); dirVec = 0;}
        return rc;
        } else curEnt = 0;

   return XrdOssOK;
}

/******************************************************************************/
/*                               r e a d d i r                                */
/******************************************************************************/

/*
  Function: Read the next entry if directory associated with this object.

  Input:    buff       - Is the address of the buffer that is to hold the next
                         directory name.
            blen       - Size of the buffer.

  Output:   Upon success, places the contents of the next directory entry
            in buff. When the end of the directory is encountered buff
            will be set to the null string.

            Upon failure, returns a (-errno).

  Warning: The caller must provide proper serialization.
*/
int XrdPssDir::Readdir(char *buff, int blen)
{
// Check if we are directly reading the directory
//
   if (myDir)
      {dirent *entP, myEnt;
       int    rc = XrdPosixXrootd::Readdir_r(myDir, &myEnt, &entP);
       if (rc) return -rc;
       if (!entP) *buff = 0;
          else strlcpy(buff, myEnt.d_name, blen);
       return XrdOssOK;
      }

// Check if this object is actually open
//
   if (!dirVec) return -XRDOSS_E8002;

// Return a single entry
//
   if (curEnt >= numEnt) *buff = 0;
      else {strlcpy(buff, dirVec[curEnt], blen);
            free(dirVec[curEnt]);
            curEnt++;
           }
   return XrdOssOK;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/
  
/*
  Function: Close the directory associated with this object.

  Input:    None.

  Output:   Returns XrdOssOK upon success and (errno) upon failure.
*/
int XrdPssDir::Close(long long *retsz)
{
   int i;

// Close the directory proper if it exists
//
   if (myDir)
      {if (XrdPosixXrootd::Closedir(myDir)) return -errno;
       myDir = 0;
       return XrdOssOK;
      }

// Make sure this object is open
//
   if (!dirVec) return -XRDOSS_E8002;

// Free up remaining storage
//
   for (i = curEnt; i < numEnt; i++) free(dirVec[i]);
   free(dirVec);
   dirVec = 0;
   return XrdOssOK;
}

/******************************************************************************/
/*                     o o s s _ F i l e   M e t h o d s                      */
/******************************************************************************/
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

/*
  Function: Open the file `path' in the mode indicated by `Mode'.

  Input:    path      - The fully qualified name of the file to open.
            Oflag     - Standard open flags.
            Mode      - Create mode (i.e., rwx).
            env       - Environmental information.

  Output:   XrdOssOK upon success; -errno otherwise.
*/
int XrdPssFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &Env)
{
   unsigned long long popts = XrdPssSys::XPList.Find(path);
   const char *Cgi;
   char pbuff[PBsz], cbuff[CBsz];
   int CgiLen, retc;

// Return an error if the object is already open
//
   if (fd >= 0) return -XRDOSS_E8003;

// If we are opening this in r/w mode make sure we actually can
//
   if ((Oflag & (O_WRONLY | O_RDWR | O_APPEND)) && (popts & XRDEXP_NOTRW))
      {if (popts & XRDEXP_FORCERO) Oflag = O_RDONLY;
          else return -EROFS;
      }

// Setup any required cgi information. Don't mess with it if it's an objectid
// or if the we are an outgoing proxy server.
//
   Cgi= Env.Env(CgiLen);
   if (!XrdPssSys::outProxy && *path == '/')
      {if (!(XRDEXP_STAGE & popts))
          {if (!CgiLen) {Cgi = osslclCGI; CgiLen = osslclCGL;}
              else      {Cgi = XrdPssSys::P2CGI(CgiLen, cbuff, sizeof(cbuff),
                                                Cgi, osslclCGI);
                         if (!Cgi) return -ENAMETOOLONG;
                        }
          }
      }

// Convert path to URL
//
   if (!XrdPssSys::P2URL(retc, pbuff, PBsz, path, 0, Cgi, CgiLen, tident))
      return retc;

// Try to open and if we failed, return an error
//
   if ((fd = XrdPosixXrootd::Open(pbuff,Oflag,Mode)) < 0) return -errno;

// All done
//
   return XrdOssOK;
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

/*
  Function: Close the file associated with this object.

  Input:    None.

  Output:   Returns XrdOssOK upon success aud -errno upon failure.
*/
int XrdPssFile::Close(long long *retsz)
{
    if (fd < 0) return -XRDOSS_E8004;
    if (retsz) *retsz = 0;
    if (XrdPosixXrootd::Close(fd)) return -errno;
    fd = -1;
    return XrdOssOK;
}

/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

/*
  Function: Preread `blen' bytes from the associated file.

  Input:    offset    - The absolute 64-bit byte offset at which to read.
            blen      - The size to preread.

  Output:   Returns zero read upon success and -errno upon failure.
*/

ssize_t XrdPssFile::Read(off_t offset, size_t blen)
{
     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

     return 0;  // We haven't implemented this yet!
}


/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

/*
  Function: Read `blen' bytes from the associated file, placing in 'buff'
            the data and returning the actual number of bytes read.

  Input:    buff      - Address of the buffer in which to place the data.
            offset    - The absolute 64-bit byte offset at which to read.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be read.

  Output:   Returns the number bytes read upon success and -errno upon failure.
*/

ssize_t XrdPssFile::Read(void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

     return (retval = XrdPosixXrootd::Pread(fd, buff, blen, offset)) < 0
            ? (ssize_t)-errno : retval;
}

/******************************************************************************/
/*                                  r e a d v                                 */
/******************************************************************************/

ssize_t XrdPssFile::ReadV(XrdOucIOVec     *readV,     // In
                          int              readCount) // In
/*
  Function: Perform all the reads specified in the readV vector.

  Input:    readV     - A description of the reads to perform; includes the
                        absolute offset, the size of the read, and the buffer
                        to place the data into.
            readCount - The size of the readV vector.

  Output:   Returns the number of bytes read upon success and -errno upon failure.
            If the number of bytes read is less than requested, it is considered
            an error.
*/
{
    ssize_t retval;

    if (fd < 0) return (ssize_t)-XRDOSS_E8004;

    return (retval = XrdPosixXrootd::VRead(fd, readV, readCount)) < 0 ? (ssize_t)-errno : retval;;
}

/******************************************************************************/
/*                               R e a d R a w                                */
/******************************************************************************/

/*
  Function: Read `blen' bytes from the associated file, placing in 'buff'
            the data and returning the actual number of bytes read.

  Input:    buff      - Address of the buffer in which to place the data.
            offset    - The absolute 64-bit byte offset at which to read.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be read.

  Output:   Returns the number bytes read upon success and -errno upon failure.
*/

ssize_t XrdPssFile::ReadRaw(void *buff, off_t offset, size_t blen)
{
     return Read(buff, offset, blen);
}

/******************************************************************************/
/*                                 w r i t e                                  */
/******************************************************************************/

/*
  Function: Write `blen' bytes to the associated file, from 'buff'
            and return the actual number of bytes written.

  Input:    buff      - Address of the buffer from which to get the data.
            offset    - The absolute 64-bit byte offset at which to write.
            blen      - The number of bytes to write from the buffer.

  Output:   Returns the number of bytes written upon success and -errno o/w.
*/

ssize_t XrdPssFile::Write(const void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

     return (retval = XrdPosixXrootd::Pwrite(fd, buff, blen, offset)) < 0
            ? (ssize_t)-errno : retval;
}

/******************************************************************************/
/*                                 f s t a t                                  */
/******************************************************************************/

/*
  Function: Return file status for the associated file.

  Input:    buff      - Pointer to buffer to hold file status.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

int XrdPssFile::Fstat(struct stat *buff)
{
    if (fd < 0) return -XRDOSS_E8004;

    return (XrdPosixXrootd::Fstat(fd, buff) ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                               f s y n c                                    */
/******************************************************************************/

/*
  Function: Synchronize associated file.

  Input:    None.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdPssFile::Fsync(void)
{
    if (fd < 0) return -XRDOSS_E8004;

    return (XrdPosixXrootd::Fsync(fd) ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                             f t r u n c a t e                              */
/******************************************************************************/

/*
  Function: Set the length of associated file to 'flen'.

  Input:    flen      - The new size of the file.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    If 'flen' is smaller than the current size of the file, the file
            is made smaller and the data past 'flen' is discarded. If 'flen'
            is larger than the current size of the file, a hole is created
            (i.e., the file is logically extended by filling the extra bytes 
            with zeroes).

            If compiled w/o large file support, only lower 32 bits are used.
            used.

            Currently not supported for proxies.
*/
int XrdPssFile::Ftruncate(unsigned long long flen)
{
    if (fd < 0) return -XRDOSS_E8004;

    return (XrdPosixXrootd::Ftruncate(fd, flen) ?  -errno : XrdOssOK);
}

/******************************************************************************/
/*                               g e t M m a p                                */
/******************************************************************************/
  
/*
  Function: Indicate whether or not file is memory mapped.

  Input:    addr      - Points to an address which will receive the location
                        memory where the file is mapped. If the address is
                        null, true is returned if a mapping exist.

  Output:   Returns the size of the file if it is memory mapped (see above).
            Otherwise, zero is returned and addr is set to zero.
*/
off_t XrdPssFile::getMmap(void **addr)   // Not Supported for proxies
{
   if (addr) *addr = 0;
   return 0;
}
  
/******************************************************************************/
/*                          i s C o m p r e s s e d                           */
/******************************************************************************/
  
/*
  Function: Indicate whether or not file is compressed.

  Input:    cxidp     - Points to a four byte buffer to hold the compression
                        algorithm used if the file is compressed or null.

  Output:   Returns the region size which is 0 if the file is not compressed.
            If cxidp is not null, the algorithm is returned only if the file
            is compressed.
*/
int XrdPssFile::isCompressed(char *cxidp)  // Not supported for proxies
{
    return 0;
}

/******************************************************************************/
/*                                 P 2 C G I                                  */
/******************************************************************************/

const char *XrdPssSys::P2CGI(int &cgilen, char *cbuff, int cblen,
                             const char *Cgi1, const char *Cgi2)
{

// If there is no existing cgi return the suffix only
//
   if (!Cgi1) {cgilen = strlen(Cgi2); return Cgi2;}
   if (!Cgi2) return Cgi1;

// Compose the two cgi elements together
//
   cgilen = snprintf(cbuff, cblen, "%s&%s", Cgi1, Cgi2);
   return (cgilen >= cblen ? 0 : cbuff);
}
  
/******************************************************************************/
/*                                 P 2 D S T                                  */
/******************************************************************************/

int XrdPssSys::P2DST(int &retc, char *hBuff, int hBlen, XrdPssSys::PolAct pEnt,
                     const char *path)
{
   const char *Slash;
   int n;

// Extract out the destination
//
   Slash = index(path, '/');
   if (!Slash || (n = (Slash - path)) == 0) {retc = -EINVAL; return 0;}
   if (n >= hBlen) {retc = -ENAMETOOLONG; return 0;}
   strncpy(hBuff, path, n); hBuff[n] = 0;

// Check if we need to authorize the outgoing connection
//
   if (Police[pEnt] && !Police[pEnt]->Authorize(hBuff))
      {retc = -EACCES; return 0;}

// All is well
//
   return n;
}

/******************************************************************************/
/*                                 P 2 O U T                                  */
/******************************************************************************/
  
char *XrdPssSys::P2OUT(int &retc,  char *pbuff, int pblen,
                 const char *path, const char *Cgi, const char *Ident)
{
   char  hBuff[288], idBuff[8], *idP;
   const char *theID = "", *Quest = "", *thePath = path;
   int n;

// If we have an Ident then use the fd number as the userid. This allows us to
// have one stream per open connection.
//
   if (Ident && (Ident = index(Ident, ':')))
      {strncpy(idBuff, Ident+1, 7); idBuff[7] = 0;
       if ((idP = index(idBuff, '@'))) {*(idP+1) = 0; theID = idBuff;}
      }

// Prehandle the cgi information
//
   if (Cgi) Quest = "?";
      else  Cgi   = "";

// Make sure the path is valid for an outgoing proxy
//
   if (*path == '/') path++;
   if (*path == 'x') path++;
   if (!strncmp("root:/", path, 6)) path += 6;
      else {if (!hdrLen) {retc = -ENOTSUP; return 0;}
            n = snprintf(pbuff, pblen, hdrData, theID, thePath, Quest, Cgi);
            if (n >= pblen) {retc = -ENAMETOOLONG; return 0;}
            return pbuff;
           }

// Objectid must be handled differently as they have not been refalgomized
//
   if (*thePath != '/')
      {if (*path == '/') 
          {path++;
           if (*path == '/') theID = "";
          }
       if (Police[PolObj] && !P2DST(retc, hBuff, sizeof(hBuff), PolObj,
                                    path+(*path == '/' ? 1:0))) return 0;
       n = snprintf(pbuff, pblen, "root://%s%s%s%s", theID, path, Quest, Cgi);
       if (n >= pblen) {retc = -ENAMETOOLONG; return 0;}
       return pbuff;
      }

// Extract out the destination. We need to do this because the front end
// will have extracted out double slashes and we need to add them back. We
// also authorize the outgoing connection if we need to in the process.
//
   if (!(n = P2DST(retc, hBuff, sizeof(hBuff), PolPath, path))) return 0;
   path += n;

// Create the new path
//
   n = snprintf(pbuff,pblen,"root://%s%s/%s%s%s",theID,hBuff,path,Quest,Cgi);

// Make sure the path will fit
//
   if (n > pblen) {retc = -ENAMETOOLONG; return 0;}

// All done
//
   return pbuff;
}

/******************************************************************************/
/*                                 P 2 U R L                                  */
/******************************************************************************/
  
char *XrdPssSys::P2URL(int &retc, char *pbuff, int pblen,
                 const char *path,  int Split,
                 const char *Cgi,   int CgiLn, const char *Ident,int doN2N)
{
   int   pfxLen, pathln;
   const char *theID = 0, *subPath;
   const char *fname = path;
   char  idBuff[8], *idP, *retPath;
   char  Apath[MAXPATHLEN*2+1];

// If this is an outgoing proxy then we need to do someother work
//
   if (outProxy) return P2OUT(retc, pbuff, pblen, path, Cgi, Ident);

// First, apply the N2N mapping if necessary. If N2N fails then the whole
// mapping fails and ENAMETOOLONG will be returned.
//
   if (doN2N && XrdProxySS.theN2N)
      {if ((retc = XrdProxySS.theN2N->lfn2pfn(path, Apath, sizeof(Apath))))
          return 0;
       fname = Apath;
      }
   pathln = strlen(fname);

// If we have an Ident then usethe fd number as the userid. This allows us to
// have one stream per open connection.
//
   if (Ident && (Ident = index(Ident, ':')))
      {strncpy(idBuff, Ident+1, 7); idBuff[7] = 0;
       if ((idP = index(idBuff, '@'))) {*(idP+1) = 0; theID = idBuff;}
      }

// Format the header into the buffer and check if we overflowed. Note that there
// can be a maximum of 8 substitutions, so that's how many we provide.
//
   if (theID) pfxLen = snprintf(pbuff,pblen,hdrData,theID,theID,theID,theID,
                                                    theID,theID,theID,theID);
      else if ((pfxLen = urlPlen) < pblen) strcpy(pbuff, urlPlain);

// Calculate if the rest of the data will actually fit (we overestimate by 1)
//
   if ((pfxLen + pathln + CgiLn + 1 + (Split ? 1 : 0)) >= pblen)
      {retc = -ENAMETOOLONG;
       return 0;
      }
   retPath = (pbuff += pfxLen);
   retc = 0;

// If we need to return a split path, then compute where to split it. Note
// that Split assumes that all redundant slashes have been removed. We do
// not add any cgi information if the split fails.
//
   if (Split)
      {if ((subPath = rindex(fname+1, '/')) && *(subPath+1))
          {int n = subPath-fname;
           strncpy(retPath, fname, n); retPath += n; *retPath++ = 0;
           strcpy(retPath, subPath);
           pathln++;
          } else {
           strcpy(retPath, fname);
           return retPath+pathln;
          }
       } else strcpy(retPath, fname);

// Add any cgi information
//
   if (CgiLn)
      {idP = retPath + pathln;
       *idP++ = '?';
       strcpy(idP, Cgi);
      }

   return retPath;
}

/******************************************************************************/
/*                                 T 2 U I D                                  */
/******************************************************************************/
  
int XrdPssSys::T2UID(const char *Ident)
{
   char *Eol;

// We will use the FD number as the userid. If we fail, use ours
//
   if (Ident && (Ident = index(Ident, ':')))
      {int theUid = strtol(Ident+1, &Eol, 10);
       if (*Eol == '@') return theUid;
      }
   return myUid;
}
