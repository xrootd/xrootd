/******************************************************************************/
/*                                                                            */
/*                          X r d O s s A p i . c c                           */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/*                             i n c l u d e s                                */
/******************************************************************************/
  
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <strings.h>
#include <cstdio>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef __solaris__
#include <sys/vnode.h>
#endif

#include "XrdVersion.hh"

#include "XrdFrc/XrdFrcXAttr.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssCache.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssMio.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucXAttr.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPlugin.hh"

#ifdef XRDOSSCX
#include "oocx_CXFile.h"
#endif

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/

XrdOssSys  *XrdOssSS = 0;
  
XrdSysError OssEroute(0, "oss_");

XrdSysTrace OssTrace("oss");

/******************************************************************************/
/*           S t o r a g e   S y s t e m   I n s t a n t i a t o r            */
/******************************************************************************/

char      XrdOssSys::tryMmap = 0;
char      XrdOssSys::chkMmap = 0;

/******************************************************************************/
/*                XrdOssGetSS (a.k.a. XrdOssGetStorageSystem)                 */
/******************************************************************************/
  
// This function is called by the OFS layer to retrieve the Storage System
// object. If a plugin library has been specified, then this function will
// return the object provided by XrdOssGetStorageSystem() within the library.
//
XrdOss *XrdOssGetSS(XrdSysLogger *Logger, const char *config_fn,
                    const char   *OssLib, const char *OssParms,
                    XrdOucEnv    *envP,   XrdVersionInfo &urVer)
{
   static XrdOssSys   myOssSys;
   extern XrdSysError OssEroute;
   XrdOucPinLoader *myLib;
   XrdOss          *ossP;

// Verify that versions are compatible.
//
   if (urVer.vNum != myOssSys.myVersion->vNum
   &&  !XrdSysPlugin::VerCmp(urVer, *(myOssSys.myVersion))) return 0;

// Set logger for tracing and errors
//
   OssTrace.SetLogger(Logger);
   OssEroute.logger(Logger);

// If no library has been specified, return the default object
//
   if (!OssLib) {if (myOssSys.Init(Logger, config_fn, envP)) return 0;
                    else return (XrdOss *)&myOssSys;
                }

// Create a plugin object. Take into account the proxy library. Eventually,
// we will need to support other core libraries. But, for now, this will do.
//
   if (!(myLib = new XrdOucPinLoader(&OssEroute, myOssSys.myVersion,
                                     "osslib",   OssLib))) return 0;
// Declare the interface versions
//
   XrdOssGetStorageSystem_t  getOSS1;
   const char               *epName1 = "XrdOssGetStorageSystem";
   XrdOssGetStorageSystem2_t getOSS2;
   const char               *epName2 ="?XrdOssGetStorageSystem2";

// First try finding version 2 of the initializer. If that fails try version 1.
// In the process, we will get an oss object if we succeed at all.
//
   getOSS2 = (XrdOssGetStorageSystem2_t)myLib->Resolve(epName2);
   if (getOSS2) ossP = getOSS2((XrdOss *)&myOssSys, Logger,   config_fn,
                                                    OssParms, envP);
      else {getOSS1 = (XrdOssGetStorageSystem_t)myLib->Resolve(epName1);
            if (!getOSS1) return 0;
            ossP = getOSS1((XrdOss *)&myOssSys, Logger, config_fn, OssParms);
           }

// Call the legacy EnvInfo() method and set what library we are using if it
// differs from what we wre passed.
//
   if (ossP && envP)
      {ossP->EnvInfo(envP);
       if (envP && strcmp(OssLib, myLib->Path()))
          envP->Put("oss.lib", myLib->Path());
      }

// All done
//
   delete myLib;
   return ossP;
}
 
/******************************************************************************/
/*                       X r d O s s D e f a u l t S S                        */
/******************************************************************************/
  
 XrdOss *XrdOssDefaultSS(XrdSysLogger   *logger,
                         const char     *cfg_fn,
                         XrdVersionInfo &urVer)
{
   return XrdOssGetSS(logger, cfg_fn, 0, 0, 0, urVer);
}

/******************************************************************************/
/*                      o o s s _ S y s   M e t h o d s                       */
/******************************************************************************/
/******************************************************************************/
/*                                  i n i t                                   */
/******************************************************************************/
  
/*
  Function: Initialize staging subsystem

  Input:    None

  Output:   Returns zero upon success otherwise (-errno).
*/
int XrdOssSys::Init(XrdSysLogger *lp, const char *configfn, XrdOucEnv *envP)
{
     int retc;

// No need to do the herald thing as we are the default storage system
//
   OssEroute.logger(lp);

// Initialize the subsystems
//
   XrdOssSS = this;
   if ( (retc = Configure(configfn, OssEroute, envP)) ) return retc;

// All done.
//
   return XrdOssOK;
}

/******************************************************************************/
/*                               L f n 2 P f n                                */
/******************************************************************************/
  
int XrdOssSys::Lfn2Pfn(const char *oldp, char *newp, int blen)
{
    if (lcl_N2N) return -(lcl_N2N->lfn2pfn(oldp, newp, blen));
    if ((int)strlen(oldp) >= blen) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
}

const char *XrdOssSys::Lfn2Pfn(const char *oldp, char *newp, int blen, int &rc)
{
    if (!lcl_N2N) {rc = 0; return oldp;}
    if ((rc = -(lcl_N2N->lfn2pfn(oldp, newp, blen)))) return 0;
    return newp;
}

/******************************************************************************/
/*                          G e n L o c a l P a t h                           */
/******************************************************************************/
  
/* GenLocalPath() generates the path that a file will have in the local file
   system. The decision is made based on the user-given path (typically what 
   the user thinks is the local file system path). The output buffer where the 
   new path is placed must be at least MAXPATHLEN bytes long.
*/
int XrdOssSys::GenLocalPath(const char *oldp, char *newp)
{
    if (lcl_N2N) return -(lcl_N2N->lfn2pfn(oldp, newp, MAXPATHLEN));
    if (strlen(oldp) >= MAXPATHLEN) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
}

/******************************************************************************/
/*                         G e n R e m o t e P a t h                          */
/******************************************************************************/
  
/* GenRemotePath() generates the path that a file will have in the remote file
   system. The decision is made based on the user-given path (typically what 
   the user thinks is the local file system path). The output buffer where the 
   new path is placed must be at least MAXPATHLEN bytes long.
*/
int XrdOssSys::GenRemotePath(const char *oldp, char *newp)
{
    if (rmt_N2N) return -(rmt_N2N->lfn2rfn(oldp, newp, MAXPATHLEN));
    if (strlen(oldp) >= MAXPATHLEN) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
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

  Notes:    Files are only changed in the local disk cache.
*/

int XrdOssSys::Chmod(const char *path, mode_t mode, XrdOucEnv *envP)
{
    char actual_path[MAXPATHLEN+1], *local_path;
    int retc;

// Generate local path
//
   if (lcl_N2N)
      if ((retc = lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path)))) 
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Change the file only in the local filesystem.
//
   return (chmod(local_path, mode) ? -errno : XrdOssOK);
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
*/

int XrdOssSys::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *envP)
{
    char actual_path[MAXPATHLEN+1], *local_path;
    int retc;

// Make sure we can modify this path
//
   Check_RW(Mkdir, path, "create directory");

// Generate local path
//
   if (lcl_N2N)
      if ((retc = lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path)))) 
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Create the directory or full path only in the loal file system
//
   if (!mkdir(local_path, mode))  return XrdOssOK;
   if (mkpath && errno == ENOENT){return Mkpath(local_path, mode);}
                                  return -errno;
}

/******************************************************************************/
/*                                M k p a t h                                 */
/******************************************************************************/
/*
  Function: Create a directory path

  Input:    path        - Is the fully qualified *local* name of the new path.
            mode        - The new mode that each new directory is to have.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    Directories are only created in the local disk cache.
*/

int XrdOssSys::Mkpath(const char *path, mode_t mode)
{
    char local_path[MAXPATHLEN+1], *next_path;
    int  i = strlen(path);

// Copy the path so we can modify it
//
   strcpy(local_path, path);

// Trim off the trailing slashes so we can have predictable behaviour
//
   while(i && local_path[--i] == '/') local_path[i] = '\0';
   if (!i) return -ENOENT;

// Start creating directories starting with the root
//
   next_path = local_path;
   while((next_path = index(next_path+1, int('/'))))
        {*next_path = '\0';
         if (mkdir(local_path, mode) && errno != EEXIST) return -errno;
         *next_path = '/';
        }

// Create last component and return
//
   if (mkdir(local_path, mode) && errno != EEXIST) return -errno;
   return XrdOssOK;
}
  

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

/*
  Function: Return statistics.

  Input:    buff        - Buffer where the statistics are to be placed.
            blen        - The length of the buffer.

  Output:   Returns number of bytes placed in the buffer less null byte.
*/

int XrdOssSys::Stats(char *buff, int blen)
{
   static const char statfmt1[] = "<stats id=\"oss\" v=\"2\">";
   static const char statfmt2[] = "</stats>";
   static const int  statflen = sizeof(statfmt1) + sizeof(statfmt2);
   char *bp = buff;
   int n;

// If only size wanted, return what size we need
//
   if (!buff) return statflen + getStats(0,0);

// Make sure we have enough space
//
   if (blen < statflen) return 0;
   strcpy(bp, statfmt1);
   bp += sizeof(statfmt1)-1; blen -= sizeof(statfmt1)-1;

// Generate space statistics
//
   n = getStats(bp, blen);
   bp += n; blen -= n;

// Add trailer
//
   if (blen >= (int)sizeof(statfmt2))
      {strcpy(bp, statfmt2); bp += (sizeof(statfmt2)-1);}
   return bp - buff;
}
  
/******************************************************************************/
/*                              T r u n c a t e                               */
/******************************************************************************/

/*
  Function: Truncate a file.

  Input:    path        - Is the fully qualified name of the target file.
            size        - The new size that the file is to have.
            envP        - Environmental information.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    Files are only changed in the local disk cache.
*/

int XrdOssSys::Truncate(const char *path, unsigned long long size,
                        XrdOucEnv *envP)
{
    struct stat statbuff;
    char actual_path[MAXPATHLEN+1], *local_path;
    long long oldsz;
    int retc;

// Make sure we can modify this path
//
   Check_RW(Truncate, path, "truncate");

// Generate local path
//
   if (lcl_N2N)
      if ((retc = lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path)))) 
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Get file info to do the correct adjustment
//
   if (lstat(local_path, &statbuff)) return -errno;
       else if ((statbuff.st_mode & S_IFMT) == S_IFDIR) return -EISDIR;
       else if ((statbuff.st_mode & S_IFMT) == S_IFLNK)
               {struct stat buff;
                if (stat(local_path, &buff)) return -errno;
                oldsz = buff.st_size;
               } else oldsz = statbuff.st_size;

// Change the file only in the local filesystem and make space adjustemt
//
   if (truncate(local_path, size)) return -errno;
   XrdOssCache::Adjust(local_path,static_cast<long long>(size)-oldsz,&statbuff);
   return XrdOssOK;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                      o o s s _ D i r   M e t h o d s                       */
/******************************************************************************/
/******************************************************************************/
/*                               o p e n d i r                                */
/******************************************************************************/
  
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.
            env       - Environmental information.

  Output:   Returns XrdOssOK upon success; (-errno) otherwise.
*/
int XrdOssDir::Opendir(const char *dir_path, XrdOucEnv &Env)
{
   EPNAME("Opendir");
   char actual_path[MAXPATHLEN+1], *local_path, *remote_path;
   int retc;

// Return an error if this object is already open
//
   if (isopen) return -XRDOSS_E8001;

// Get the processing flags for this directory
//
   unsigned long long pflags = XrdOssSS->PathOpts(dir_path);
   if (pflags & XRDEXP_STAGE)   dOpts |= isStage;
   if (pflags & XRDEXP_NODREAD) dOpts |= noDread;
   if (pflags & XRDEXP_NOCHECK) dOpts |= noCheck;
   ateof = false;

// Generate local path
//
   if (XrdOssSS->lcl_N2N)
      if ((retc = XrdOssSS->lcl_N2N->lfn2pfn(dir_path, actual_path, sizeof(actual_path))))
         return retc;
         else local_path = actual_path;
      else local_path = (char *)dir_path;

// If this is a local filesystem request, open locally. We also obtian the
// underlying file descriptor.
//
   if (!(dOpts & isStage) || (dOpts & noDread))
      {TRACE(Opendir, "lcl path " <<local_path <<" (" <<dir_path <<")");
       if (!(lclfd = XrdSysFD_OpenDir(local_path))) return -errno;
       fd = dirfd(lclfd);
       isopen = true;
       return XrdOssOK;
      }

// Generate remote path
//
   if (XrdOssSS->rmt_N2N)
      if ((retc = XrdOssSS->rmt_N2N->lfn2rfn(dir_path, actual_path, sizeof(actual_path))))
         return retc;
         else remote_path = actual_path;
      else remote_path = (char *)dir_path;

   TRACE(Opendir, "rmt path " << remote_path <<" (" << dir_path <<")");

// Originally, if MSS directories were not to be read, we ould simply check
// if the path was a directory and return an error if not. That was superceeded
// by making NODREAD mean to read the local directory only (which is not always
// ideal). So, we keep the code below but comment it out for now.
//
// if ((dOpts & noDread) && !(dOpts & noCheck))
//    {struct stat fstat;
//     if ((retc = XrdOssSS->MSS_Stat(remote_path,&fstat))) return retc;
//     if (!(S_ISDIR(fstat.st_mode))) return -ENOTDIR;
//     isopen = true;
//     return XrdOssOK;
//    }

// Open the directory at the remote location.
//
   if (!(mssfd = XrdOssSS->MSS_Opendir(remote_path, retc))) return retc;
   isopen = true;
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
int XrdOssDir::Readdir(char *buff, int blen)
{
   struct dirent *rp;

// Check if this object is actually open
//
   if (!isopen) return -XRDOSS_E8002;

// Perform local reads if this is a local directory
//
   if (lclfd)
      {errno = 0;
       while((rp = readdir(lclfd)))
            {strlcpy(buff, rp->d_name, blen);
#ifdef HAVE_FSTATAT
             if (Stat && fstatat(fd, rp->d_name, Stat, 0))
                {if (errno != ENOENT) return -errno;
                 errno = 0;
                 continue;
                }
#endif
             return XrdOssOK;
            }
       *buff = '\0'; ateof = true;
       return -errno;
      }

// Simulate the read operation, if need be.
//
   if (noDread)
      {if (ateof) *buff = '\0';
          else   {*buff = '.'; ateof = true;}
       return XrdOssOK;
      }

// Perform a remote read
//
   return XrdOssSS->MSS_Readdir(mssfd, buff, blen);
}

/******************************************************************************/
/*                               S t a t R e t                                */
/******************************************************************************/
/*
  Function: Set stat buffer pointerto automatically stat returned entries.

  Input:    buff       - Pointer to the stat buffer.

  Output:   Upon success, return 0.

            Upon failure, returns a (-errno).

  Warning: The caller must provide proper serialization.
*/
int XrdOssDir::StatRet(struct stat *buff)
{

// Check if this object is actually open
//
   if (!isopen) return -XRDOSS_E8002;

// We only support autostat for local directories
//
   if (!lclfd) return -ENOTSUP;

// We do not support autostat unless we have the fstatat function
//
#ifndef HAVE_FSTATAT
   return -ENOTSUP;
#endif

// All is well
//
   Stat = buff;
   return 0;
}
  
/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/
  
/*
  Function: Close the directory associated with this object.

  Input:    None.

  Output:   Returns XrdOssOK upon success and (errno) upon failure.
*/
int XrdOssDir::Close(long long *retsz)
{
    int retc;

// We do not support returing a size
//
   if (retsz) *retsz = 0;

// Make sure this object is open
//
    if (!isopen) return -XRDOSS_E8002;

// Close whichever handle is open
//
    if (lclfd)
       {if (!(retc = closedir(lclfd)))
           {lclfd = 0;
            isopen = false;
           }
       } else {
        if (mssfd) { if (!(retc = XrdOssSS->MSS_Closedir(mssfd))) mssfd = 0;}
           else retc = 0;
       }

// Indicate whether or not we really closed this object
//
   return retc;
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
int XrdOssFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &Env)
{
   unsigned long long popts;
   int retc, mopts;
   char actual_path[MAXPATHLEN+1], *local_path;
   struct stat buf;

// Return an error if this object is already open
//
   if (fd >= 0) return -XRDOSS_E8003;
      else cxobj = 0;

// Construct the processing options for this path.
//
   popts = XrdOssSS->PathOpts(path);
   if (popts & XRDEXP_STAGE && Env.Get("oss.lcl")) popts &= ~XRDEXP_STAGE;

// Generate local path
//
   if (XrdOssSS->lcl_N2N)
      if ((retc = XrdOssSS->lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path))))
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Check if this is a read/only filesystem
//
   if ((Oflag & (O_WRONLY | O_RDWR)) && (popts & XRDEXP_NOTRW))
      {if (popts & XRDEXP_FORCERO) Oflag = O_RDONLY;
          else return OssEroute.Emsg("Open",-XRDOSS_E8005,"open r/w",path);
      }

// If we can open the local copy. If not found, try to stage it in if possible.
// Note that stage will regenerate the right local and remote paths.
//
   if ( (fd = (int)Open_ufs(local_path, Oflag, Mode, popts)) == -ENOENT
   && (popts & XRDEXP_REMOTE))
      {if (!(popts & XRDEXP_STAGE))
          return OssEroute.Emsg("Open",-XRDOSS_E8006,"open",path);
       if ((retc = XrdOssSS->Stage(tident, path, Env, Oflag, Mode, popts)))
          return retc;
       fd = (int)Open_ufs(local_path, Oflag, Mode, popts & ~XRDEXP_REMOTE);
      }

// This interface supports only regular files. Complain if this is not one.
//
   if (fd >= 0)
      {do {retc = fstat(fd, &buf);} while(retc && errno == EINTR);
       if (!retc && !(buf.st_mode & S_IFREG))
          {close(fd); fd = (buf.st_mode & S_IFDIR ? -EISDIR : -ENOTBLK);}
       if (Oflag & (O_WRONLY | O_RDWR))
          {FSize = buf.st_size; cacheP = XrdOssCache::Find(local_path);}
          else {if (buf.st_mode & XRDSFS_POSCPEND && fd >= 0)
                   {close(fd); fd=-ETXTBSY;}
                FSize = -1; cacheP = 0;
               }
      } else if (fd == -EEXIST)
                {do {retc = stat(local_path,&buf);} while(retc && errno==EINTR);
                 if (!retc && (buf.st_mode & S_IFDIR)) fd = -EISDIR;
                }

// See if should memory map this file. For now, extended attributes are only
// needed when memory mapping is enabled and can apply only to specific files.
// So, we read them here should we need them.
//
   if (fd >= 0 && XrdOssSS->tryMmap)
      {XrdOucXAttr<XrdFrcXAttrMem> Info;
       mopts = 0;
       if (!(popts & XRDEXP_NOXATTR) && XrdOssSS->chkMmap)
          Info.Get(local_path, fd);
       if (popts & XRDEXP_MKEEP || Info.Attr.Flags & XrdFrcXAttrMem::memKeep)
          mopts |= OSSMIO_MPRM;
       if (popts & XRDEXP_MLOK  || Info.Attr.Flags & XrdFrcXAttrMem::memLock)
          mopts |= OSSMIO_MLOK;
       if (popts & XRDEXP_MMAP  || Info.Attr.Flags & XrdFrcXAttrMem::memMap)
          mopts |= OSSMIO_MMAP;
       if (mopts) mmFile = XrdOssMio::Map(local_path, fd, mopts);
      } else mmFile = 0;

// Return the result of this open
//
   return (fd < 0 ? fd : XrdOssOK);
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

/*
  Function: Close the file associated with this object.

  Input:    None.

  Output:   Returns XrdOssOK upon success and -1 upon failure.
*/
int XrdOssFile::Close(long long *retsz)
{
    if (fd < 0) return -XRDOSS_E8004;
    if (retsz || cacheP)
       {struct stat buf;
        int retc;
        do {retc = fstat(fd, &buf);} while(retc && errno == EINTR);
        if (cacheP && FSize != buf.st_size)
           XrdOssCache::Adjust(cacheP, buf.st_size - FSize);
        if (retsz) *retsz = buf.st_size;
       }
    if (close(fd)) return -errno;
    if (mmFile) {XrdOssMio::Recycle(mmFile); mmFile = 0;}
#ifdef XRDOSSCX
    if (cxobj) {delete cxobj; cxobj = 0;}
#endif
    fd = -1; FSize = -1; cacheP = 0;
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

ssize_t XrdOssFile::Read(off_t offset, size_t blen)
{

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

#if defined(__linux__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
     posix_fadvise(fd, offset, blen, POSIX_FADV_WILLNEED);
#endif

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

ssize_t XrdOssFile::Read(void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

#ifdef XRDOSSCX
     if (cxobj)  
        if (XrdOssSS->DirFlags & XrdOssNOSSDEC) return (ssize_t)-XRDOSS_E8021;
           else   retval = cxobj->Read((char *)buff, blen, offset);
        else 
#endif
             do { retval = pread(fd, buff, blen, offset); }
                while(retval < 0 && errno == EINTR);

     return (retval >= 0 ? retval : (ssize_t)-errno);
}

/******************************************************************************/
/*                                  r e a d v                                 */
/******************************************************************************/

/*
  Function: Perform all the reads specified in the readV vector.

  Input:    readV     - A description of the reads to perform; includes the
                        absolute offset, the size of the read, and the buffer
                        to place the data into.
            readCount - The size of the readV vector.

  Output:   Returns the number of bytes read upon success and SFS_ERROR o/w.
            If the number of bytes read is less than requested, it is considered
            an error.
*/

ssize_t XrdOssFile::ReadV(XrdOucIOVec *readV, int n)
{
   ssize_t rdsz, totBytes = 0;
   int i;

// For platforms that support fadvise, pre-advise what we will be reading
//
#if (defined(__linux__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))) && defined(HAVE_ATOMICS)
   EPNAME("ReadV");
   long long begOff, endOff, begLst = -1, endLst = -1;
   int nPR = n;

// Indicate we are in preread state and see if we have exceeded the limit
//
   if (XrdOssSS->prDepth
   && AtomicInc((XrdOssSS->prActive)) < XrdOssSS->prQSize && n > 2)
      {int faBytes = 0;
       for (nPR=0;nPR < XrdOssSS->prDepth && faBytes < XrdOssSS->prBytes;nPR++)
           if (readV[nPR].size > 0)
              {begOff = XrdOssSS->prPMask &  readV[nPR].offset;
               endOff = XrdOssSS->prPBits | (readV[nPR].offset+readV[nPR].size);
               rdsz = endOff - begOff + 1;
               if ((begOff > endLst || endOff < begLst)
               &&  rdsz < XrdOssSS->prBytes)
                  {posix_fadvise(fd, begOff, rdsz, POSIX_FADV_WILLNEED);
                   TRACE(Debug,"fadvise(" <<fd <<',' <<begOff <<',' <<rdsz <<')');
                   faBytes += rdsz;
                  }
               begLst = begOff; endLst = endOff;
              }
      }
#endif

// Read in the vector and do a pre-advise if we support that
//
   for (i = 0; i < n; i++)
       {do {rdsz = pread(fd, readV[i].data, readV[i].size, readV[i].offset);}
           while(rdsz < 0 && errno == EINTR);
        if (rdsz < 0 || rdsz != readV[i].size)
           {totBytes =  (rdsz < 0 ? -errno : -ESPIPE); break;}
        totBytes += rdsz;
#if (defined(__linux__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))) && defined(HAVE_ATOMICS)
        if (nPR < n && readV[nPR].size > 0)
           {begOff = XrdOssSS->prPMask &  readV[nPR].offset;
            endOff = XrdOssSS->prPBits | (readV[nPR].offset+readV[nPR].size);
            rdsz = endOff - begOff + 1;
            if ((begOff > endLst || endOff < begLst)
            &&  rdsz <= XrdOssSS->prBytes)
               {posix_fadvise(fd, begOff, rdsz, POSIX_FADV_WILLNEED);
                TRACE(Debug,"fadvise(" <<fd <<',' <<begOff <<',' <<rdsz <<')');
               }
            begLst = begOff; endLst = endOff;
           }
        nPR++;
#endif
       }

// All done, return bytes read.
//
#if (defined(__linux__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))) && defined(HAVE_ATOMICS)
   if (XrdOssSS->prDepth) AtomicDec((XrdOssSS->prActive));
#endif
   return totBytes;
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

ssize_t XrdOssFile::ReadRaw(void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

#ifdef XRDOSSCX
     if (cxobj)   retval = cxobj->ReadRaw((char *)buff, blen, offset);
        else 
#endif
             do { retval = pread(fd, buff, blen, offset); }
                while(retval < 0 && errno == EINTR);

     return (retval >= 0 ? retval : (ssize_t)-errno);
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

ssize_t XrdOssFile::Write(const void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

     if (XrdOssSS->MaxSize && (long long)(offset+blen) > XrdOssSS->MaxSize)
        return (ssize_t)-XRDOSS_E8007;

     do { retval = pwrite(fd, buff, blen, offset); }
          while(retval < 0 && errno == EINTR);

     if (retval < 0) retval = (retval == EBADF && cxobj ? -XRDOSS_E8022 : -errno);
     return retval;
}

/******************************************************************************/
/*                                F c h m o d                                 */
/******************************************************************************/

/*
  Function: Sets mode bits for an open file.

  Input:    Mode      - The mode to set.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

int XrdOssFile::Fchmod(mode_t Mode)
{
    return (fchmod(fd, Mode) ? -errno : XrdOssOK);
}
  
/******************************************************************************/
/*                                  F c t l                                   */
/******************************************************************************/
/*
  Function: Perform control operations on a file.

  Input:    cmd       - The command.
            alen      - length of arguments.
            args      - Pointer to arguments.
            resp      - Pointer to where response should be placed.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

int XrdOssFile::Fctl(int cmd, int alen, const char *args, char **resp)
{
   const struct timeval *utArgs;

   switch(cmd)
         {case XrdOssDF::Fctl_utimes:
               if (alen != sizeof(struct timeval)*2 || !args) return -EINVAL;
               utArgs = (const struct timeval *)args;
               if (futimes(fd, utArgs)) return -errno;
               return XrdOssOK;
               break;
          default: break;
         }
   return -ENOTSUP;
}
  
/******************************************************************************/
/*                                 F l u s h                                  */
/******************************************************************************/

/*
  Function: Flush file pages from the filesyste cache.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

void XrdOssFile::Flush()
{
// This actually only works in Linux so we punt otherwise
//
#if defined(__linux__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
   if (fd>= 0)
      {fdatasync(fd);
       posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
      }
#endif
}

/******************************************************************************/
/*                                 F s t a t                                  */
/******************************************************************************/

/*
  Function: Return file status for the associated file.

  Input:    buff      - Pointer to buffer to hold file status.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

int XrdOssFile::Fstat(struct stat *buff)
{
    return (fstat(fd, buff) ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                               F s y n c                                    */
/******************************************************************************/

/*
  Function: Synchronize associated file.

  Input:    None.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdOssFile::Fsync(void)
{
    return (fsync(fd) ? -errno : XrdOssOK);
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
off_t XrdOssFile::getMmap(void **addr)
{
   if (mmFile) return (addr ? mmFile->Export(addr) : 1);
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

  Output:   Returns the regios size which is 0 if the file is not compressed.
            If cxidp is not null, the algorithm is returned only if the file
            is compressed.
*/
int XrdOssFile::isCompressed(char *cxidp)
{
    if (cxpgsz)
       {cxidp[0] = cxid[0]; cxidp[1] = cxid[1];
        cxidp[2] = cxid[2]; cxidp[3] = cxid[3];
       }
    return cxpgsz;
}

/******************************************************************************/
/*                              t r u n c a t e                               */
/******************************************************************************/

/*
  Function: Set the length of associated file to 'flen'.

  Input:    flen      - The new size of the file. Only 32-bit lengths
                        are supported.

  Output:   Returns XrdOssOK upon success and -1 upon failure.

  Notes:    If 'flen' is smaller than the current size of the file, the file
            is made smaller and the data past 'flen' is discarded. If 'flen'
            is larger than the current size of the file, a hole is created
            (i.e., the file is logically extended by filling the extra bytes 
            with zeroes).

            If compiled w/o large file support, only lower 32 bits are used.
            used.
            in supporting it for any other system.
*/
int XrdOssFile::Ftruncate(unsigned long long flen) {
    off_t newlen = flen;

    if (sizeof(newlen) < sizeof(flen) && (flen>>31)) return -XRDOSS_E8008;

// Note that space adjustment will occur when the file is closed, not here
//
    return (ftruncate(fd, newlen) ?  -errno : XrdOssOK);
    }

/******************************************************************************/
/*                     P R I V A T E    S E C T I O N                         */
/******************************************************************************/
/******************************************************************************/
/*                      o o s s _ O p e n _ u f s                             */
/******************************************************************************/

int XrdOssFile::Open_ufs(const char *path, int Oflag, int Mode, 
                         unsigned long long popts)
{
    EPNAME("Open_ufs")
    static const int isWritable = O_WRONLY|O_RDWR;
    int myfd, newfd;
#ifndef NODEBUG
    char *ftype = (char *)" path=";
#endif
#ifdef XRDOSSCX
    int attcx = 0;
#endif

// If we need to do a stat() prior to the open, do so now
//
   if (XrdOssSS->STT_PreOp)
      {struct stat Stat;
       if ((*(XrdOssSS->STT_Func))(path, &Stat, XRDOSS_preop, 0)) return -errno;
      }

// Now open the actual data file in the appropriate mode.
//
    do { myfd = XrdSysFD_Open(path, Oflag|O_LARGEFILE, Mode);}
       while( myfd < 0 && errno == EINTR);

// If the file is marked purgeable or migratable and we may modify this file,
// then get a shared lock on the file to keep it from being migrated or purged
// while it is open. This is advisory so we can ignore any errors.
//
   if (myfd >= 0
   && (popts & XRDEXP_PURGE || (popts & XRDEXP_MIG && Oflag & isWritable)))
      {FLOCK_t lock_args;
       bzero(&lock_args, sizeof(lock_args));
       lock_args.l_type = F_RDLCK;
       fcntl(myfd, F_SETLKW, &lock_args);
      }

// Chck if file is compressed
//
    if (myfd < 0) myfd = -errno;
#ifdef XRDOSSCX
       else if ((popts & XRDEXP_COMPCHK)
            && oocx_CXFile::isCompressed(myfd, cxid, &cxpgsz)) 
               if (Oflag != O_RDONLY) {close(myfd); return -XRDOSS_E8022;}
                  else attcx = 1;
#endif

// Relocate the file descriptor if need be and make sure file is closed on exec
//
    if (myfd >= 0)
       {if (myfd < XrdOssSS->FDFence)
           {if ((newfd = XrdSysFD_Dup1(myfd, XrdOssSS->FDFence)) < 0)
               OssEroute.Emsg("Open_ufs",errno,"reloc FD",path);
               else {close(myfd); myfd = newfd;}
           }
#ifdef XRDOSSCX
        // If the file is compressed get a CXFile object and attach the FD to it
        //
        if (attcx) {cxobj = new oocx_CXFile;
                    ftype = (char *)" CXpath=";
                    if ((retc = cxobj->Attach(myfd, path)) < 0)
                       {close(myfd); myfd = retc; delete cxobj; cxobj = 0;}
                   }
#endif
       }

// Trace the action.
//
    TRACE(Open, "fd=" <<myfd <<" flags=" <<Xrd::hex1 <<Oflag <<" mode="
                <<Xrd::oct1 <<Mode <<ftype <<path);

// All done
//
    return myfd;
}
