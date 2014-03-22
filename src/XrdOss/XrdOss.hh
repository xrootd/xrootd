#ifndef _XRDOSS_H
#define _XRDOSS_H
/******************************************************************************/
/*                                                                            */
/*                     X r d O s s   &   X r d O s s D F                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <dirent.h>
#include <errno.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include "XrdOuc/XrdOucIOVec.hh"

class XrdOucEnv;
class XrdSysLogger;
class XrdSfsAio;

#ifndef XrdOssOK
#define XrdOssOK 0
#endif

/******************************************************************************/
/*                              X r d O s s D F                               */
/******************************************************************************/

//! This class defines the object that handles directory as well as file
//! oriented requests. It is instantiated for each file/dir to be opened.
//! The object is obtained by calling newDir() or newFile() in class XrdOss.
//! This allows flexibility on how to structure an oss plugin.
  
class XrdOssDF
{
public:
                // Directory oriented methods
virtual int     Opendir(const char *, XrdOucEnv &)           {return -ENOTDIR;}
virtual int     Readdir(char *buff, int blen)                {(void)buff; (void)blen; return -ENOTDIR;}
virtual int     StatRet(struct stat *buff)                   {(void)buff; return -ENOTSUP;}

                // File oriented methods
virtual int     Fchmod(mode_t mode)                          {(void)mode; return -EISDIR;}
virtual int     Fstat(struct stat *)                         {return -EISDIR;}
virtual int     Fsync()                                      {return -EISDIR;}
virtual int     Fsync(XrdSfsAio *aiop)                       {(void)aiop; return -EISDIR;}
virtual int     Ftruncate(unsigned long long)                {return -EISDIR;}
virtual int     getFD()                                      {return -1;}
virtual off_t   getMmap(void **addr)                         {(void)addr; return 0;}
virtual int     isCompressed(char *cxidp=0)                  {(void)cxidp; return -EISDIR;}
virtual int     Open(const char *, int, mode_t, XrdOucEnv &) {return -EISDIR;}
virtual ssize_t Read(off_t, size_t)                          {return (ssize_t)-EISDIR;}
virtual ssize_t Read(void *, off_t, size_t)                  {return (ssize_t)-EISDIR;}
virtual int     Read(XrdSfsAio *aoip)                        {(void)aoip; return (ssize_t)-EISDIR;}
virtual ssize_t ReadRaw(    void *, off_t, size_t)           {return (ssize_t)-EISDIR;}
virtual ssize_t Write(const void *, off_t, size_t)           {return (ssize_t)-EISDIR;}
virtual int     Write(XrdSfsAio *aiop)                       {(void)aiop; return (ssize_t)-EISDIR;}

// Implemented in the header, as many folks will be happy with the default.
//
virtual ssize_t ReadV(XrdOucIOVec *readV, int n)
                     {ssize_t nbytes = 0, curCount = 0;
                      for (int i=0; i<n; i++)
                          {curCount = Read((void *)readV[i].data,
                                            (off_t)readV[i].offset,
                                           (size_t)readV[i].size);
                           if (curCount != readV[i].size)
                              {if (curCount < 0) return curCount;
                               return -ESPIPE;
                              }
                           nbytes += curCount;
                          }
                      return nbytes;
                     }

// Implemented in the header, as many folks will be happy with the default.
//
virtual ssize_t WriteV(XrdOucIOVec *writeV, int n)
                      {ssize_t nbytes = 0, curCount = 0;
                       for (int i=0; i<n; i++)
                           {curCount =Write((void *)writeV[i].data,
                                             (off_t)writeV[i].offset,
                                            (size_t)writeV[i].size);
                            if (curCount != writeV[i].size)
                               {if (curCount < 0) return curCount;
                                return -ESPIPE;
                               }
                            nbytes += curCount;
                           }
                       return nbytes;
                      }

                // Methods common to both
virtual int     Close(long long *retsz=0)=0;
inline  int     Handle() {return fd;}
virtual int     Fctl(int cmd, int alen, const char *args, char **resp=0)
{
  (void)cmd; (void)alen; (void)args; (void)resp;
  return -ENOTSUP;
}

                XrdOssDF() {fd = -1;}
virtual        ~XrdOssDF() {}

protected:

int     fd;      // The associated file descriptor.
};

/******************************************************************************/
/*                                X r d O s s                                 */
/******************************************************************************/

// Options that can be passed to Create()
//
#define XRDOSS_mkpath  0x01
#define XRDOSS_new     0x02
#define XRDOSS_Online  0x04
#define XRDOSS_isPFN   0x10
#define XRDOSS_isMIG   0x20
#define XRDOSS_setnoxa 0x40

// Options that can be passed to Stat()
//
#define XRDOSS_resonly 0x0001
#define XRDOSS_updtatm 0x0002
#define XRDOSS_preop   0x0004

// Class passed to StatVS()
//
class XrdOssVSInfo
{
public:
long long Total;   // Total bytes
long long Free;    // Total bytes free
long long Large;   // Total bytes in largest partition
long long LFree;   // Max   bytes free in contiguous chunk
long long Usage;   // Used  bytes (if usage enabled)
long long Quota;   // Quota bytes (if quota enabled)
int       Extents; // Number of partitions/extents
int       Reserved;

          XrdOssVSInfo() : Total(0),Free(0),Large(0),LFree(0),Usage(-1),
                           Quota(-1),Extents(0),Reserved(0) {}
         ~XrdOssVSInfo() {}
};
  
class XrdOss
{
public:
virtual XrdOssDF *newDir(const char *tident)=0;
virtual XrdOssDF *newFile(const char *tident)=0;

virtual int     Chmod(const char *, mode_t mode, XrdOucEnv *eP=0)=0;
virtual int     Create(const char *, const char *, mode_t, XrdOucEnv &, 
                       int opts=0)=0;
virtual int     Init(XrdSysLogger *, const char *)=0;
virtual int     Mkdir(const char *, mode_t mode, int mkpath=0,
                      XrdOucEnv *eP=0)=0;
virtual int     Reloc(const char *, const char *, const char *, const char *x=0)
                      {(void)x; return -ENOTSUP;}
virtual int     Remdir(const char *, int Opts=0, XrdOucEnv *eP=0)=0;
virtual int     Rename(const char *, const char *,
                       XrdOucEnv *eP1=0, XrdOucEnv *eP2=0)=0;
virtual int     Stat(const char *, struct stat *, int opts=0, XrdOucEnv *eP=0)=0;
virtual int     StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP=0)
{ (void)path; (void)buff; (void)blen; (void)eP; return -ENOTSUP;}
virtual int     StatLS(XrdOucEnv &env, const char *cgrp, char *buff, int &blen)
{ (void)env; (void)cgrp; (void)buff; (void)blen; return -ENOTSUP;}
virtual int     StatPF(const char *, struct stat *)
                      {return -ENOTSUP;}
virtual int     StatXA(const char *path, char *buff, int &blen, XrdOucEnv *eP=0)
{ (void)path; (void)buff; (void)blen; (void)eP; return -ENOTSUP;}
virtual int     StatXP(const char *path, unsigned long long &attr,
                       XrdOucEnv *eP=0)
{ (void)path; (void)attr; (void)eP; return -ENOTSUP;}
virtual int     Truncate(const char *, unsigned long long, XrdOucEnv *eP=0)=0;
virtual int     Unlink(const char *, int Opts=0, XrdOucEnv *eP=0)=0;

virtual int     Stats(char *bp, int bl) { (void)bp; (void)bl; return 0;}

virtual int     StatVS(XrdOssVSInfo *sP, const char *sname=0, int updt=0)
{ (void)sP; (void)sname; (void)updt; return -ENOTSUP;}

virtual int     Lfn2Pfn(const char *Path, char *buff, int blen)
                       {if ((int)strlen(Path) >= blen) return -ENAMETOOLONG;
                        strcpy(buff, Path); return 0;
                       }
virtual
const char     *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
{ (void)buff; (void)blen; rc = 0; return Path;}

virtual int     FSctl(int cmd, int alen, const char *args, char **resp=0)
{ (void)cmd; (void)alen; (void)args; (void)resp; return -ENOTSUP;}

virtual void    EnvInfo(XrdOucEnv *envP) {(void)envP;}

                XrdOss() {}
virtual        ~XrdOss() {}
};

/******************************************************************************/
/*           S t o r a g e   S y s t e m   I n s t a n t i a t o r            */
/******************************************************************************/

//------------------------------------------------------------------------------
//! Get an instance of a configured XrdOss object.
//!
//! @param  native_oss -> object that would have been used as the storage
//!                       system. The object is not initialized (i.e., Init()
//!                       has not yet been called). This allows one to easily
//!                       wrap the native implementation or to completely
//!                       replace it, as needed.
//! @param  Logger     -> The message routing object to be used in conjunction
//!                       with an XrdSysError object for error messages.
//! @param  config_fn  -> The name of the config file.
//! @param  parms      -> Any parameters specified after the path on the
//!                       ofs.osslib directive. If there are no parameters, the
//!                       pointer may be zero.
//!
//! @return Success:   -> an instance of the XrdOss object to be used as the
//!                       underlying storage system.
//!         Failure:      Null pointer which causes initialization to fail.
//!
//! The object creation function must be declared as an extern "C" function
//! in the plug-in shared library as follows:
//------------------------------------------------------------------------------
/*!
    extern "C" XrdOss *XrdOssGetStorageSystem(XrdOss       *native_oss,
                                              XrdSysLogger *Logger,
                                              const char   *config_fn,
                                              const char   *parms);
*/

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdOssGetStorageSystem,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
