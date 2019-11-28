#ifndef _XRDOSS_H
#define _XRDOSS_H
/******************************************************************************/
/*                                                                            */
/*                             X r d O s s . h h                              */
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
#include <stdint.h>
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
/*                        C l a s s   X r d O s s D F                         */
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
virtual int     Readdir(char *, int)                         {return -ENOTDIR;}
virtual int     StatRet(struct stat *)                       {return -ENOTSUP;}

                // File oriented methods
virtual int     Fchmod(mode_t)                               {return -EISDIR;}
virtual void    Flush()                                      {}
virtual int     Fstat(struct stat *)                         {return -EISDIR;}
virtual int     Fsync()                                      {return -EISDIR;}
virtual int     Fsync(XrdSfsAio *)                           {return -EISDIR;}
virtual int     Ftruncate(unsigned long long)                {return -EISDIR;}
virtual int     getFD()                                      {return -1;}
virtual off_t   getMmap(void **)                             {return 0;}
virtual int     isCompressed(char *cxidp=0)                  {(void)cxidp; return -EISDIR;}
virtual int     Open(const char *, int, mode_t, XrdOucEnv &) {return -EISDIR;}
virtual ssize_t pgRead (void*, off_t, size_t, uint32_t*, uint64_t);
virtual int     pgRead (XrdSfsAio*, uint64_t);
virtual ssize_t pgWrite(void*, off_t, size_t, uint32_t*, uint64_t);
virtual int     pgWrite(XrdSfsAio*, uint64_t);
virtual ssize_t Read(off_t, size_t)                          {return (ssize_t)-EISDIR;}
virtual ssize_t Read(void *, off_t, size_t)                  {return (ssize_t)-EISDIR;}
virtual int     Read(XrdSfsAio *aoip)                        {(void)aoip; return (ssize_t)-EISDIR;}
virtual ssize_t ReadRaw(void *, off_t, size_t)               {return (ssize_t)-EISDIR;}
virtual ssize_t ReadV(XrdOucIOVec *readV, int n);
virtual ssize_t Write(const void *, off_t, size_t)           {return (ssize_t)-EISDIR;}
virtual int     Write(XrdSfsAio *aiop)                       {(void)aiop; return (ssize_t)-EISDIR;}
virtual ssize_t WriteV(XrdOucIOVec *writeV, int n);

                // Methods common to both
virtual int     Close(long long *retsz=0)=0;
inline  int     Handle() {return fd;}
virtual int     Fctl(int cmd, int alen, const char *args, char **resp=0);

                XrdOssDF() : pgwEOF(0), fd(-1) {}
virtual        ~XrdOssDF() {}

// pgRead and pgWrite options as noted.
//
static const uint64_t
Verify       = 0x8000000000000000ULL; //!< all: Verify checksums

protected:

off_t   pgwEOF;  // Highest short offset on pgWrite (0 means none yet)
int     fd;      // The associated file descriptor.
};

/******************************************************************************/
/*                        X r d O s s   O p t i o n s                         */
/******************************************************************************/

// Options that can be passed to Create()
//
#define XRDOSS_mkpath  0x01
#define XRDOSS_new     0x02
#define XRDOSS_Online  0x04
#define XRDOSS_isPFN   0x10
#define XRDOSS_isMIG   0x20
#define XRDOSS_setnoxa 0x40

// Values returned by Features()
//
#define XRDOSS_HASPGRW 0x0000000000000001ULL
#define XRDOSS_HASFSCS 0x0000000000000002ULL
#define XRDOSS_HASPRXY 0x0000000000000004ULL

// Options that can be passed to Stat()
//
#define XRDOSS_resonly 0x0001
#define XRDOSS_updtatm 0x0002
#define XRDOSS_preop   0x0004

// Commands that can be passed to FSctl
//
#define XRDOSS_FSCTLFA 0x0001

/******************************************************************************/
/*                    C l a s s   X r d O s s V S I n f o                     */
/******************************************************************************/
  
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
  
/******************************************************************************/
/*                          C l a s s   X r d O s s                           */
/******************************************************************************/
  
class XrdOss
{
public:
virtual XrdOssDF *newDir(const char *tident)=0;
virtual XrdOssDF *newFile(const char *tident)=0;

virtual int       Chmod(const char *, mode_t mode, XrdOucEnv *eP=0)=0;

virtual void      Connect(XrdOucEnv &);

virtual int       Create(const char *, const char *, mode_t, XrdOucEnv &,
                         int opts=0)=0;

virtual void      Disc(XrdOucEnv &);
virtual void      EnvInfo(XrdOucEnv *);
virtual uint64_t  Features();
virtual int       FSctl(int, int, const char *, char **x=0);

virtual int       Init(XrdSysLogger *, const char *)=0;
virtual int       Mkdir(const char *, mode_t, int mkpath=0, XrdOucEnv *eP=0)=0;

virtual int       Reloc(const char *, const char *, const char *, const char *x=0);

virtual int       Remdir(const char *, int Opts=0, XrdOucEnv *eP=0)=0;
virtual int       Rename(const char *, const char *,
                         XrdOucEnv *eP1=0, XrdOucEnv *eP2=0)=0;
virtual int       Stat(const char *, struct stat *, int opts=0, XrdOucEnv *x=0)=0;

virtual int       Stats(char *bp, int bl) { (void)bp; (void)bl; return 0;}

                  // Specialized stat type function (none supported by default)
virtual int       StatFS(const char *, char *, int &, XrdOucEnv *x=0);
virtual int       StatLS(XrdOucEnv &, const char *, char *, int &);
virtual int       StatPF(const char *, struct stat *);
virtual int       StatVS(XrdOssVSInfo *, const char *x=0, int y=0);
virtual int       StatXA(const char *, char *, int &, XrdOucEnv *x=0);
virtual int       StatXP(const char *, unsigned long long &, XrdOucEnv *x=0);

virtual int       Truncate(const char *, unsigned long long, XrdOucEnv *eP=0)=0;
virtual int       Unlink(const char *, int Opts=0, XrdOucEnv *eP=0)=0;

                  // Default Name-to-Name Methods
virtual int       Lfn2Pfn(const char *Path, char *buff, int blen)
                         {if ((int)strlen(Path) >= blen) return -ENAMETOOLONG;
                          strcpy(buff, Path); return 0;
                         }
virtual
const char       *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
                         { (void)buff; (void)blen; rc = 0; return Path;}

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
//! @param  envP       -> **Version2 Only** pointer to environmental info.
//!                       This pointer may be nil if no such information exists.
//!
//! @return Success:   -> an instance of the XrdOss object to be used as the
//!                       underlying storage system.
//!         Failure:      Null pointer which causes initialization to fail.
//!
//! The object creation function must be declared as an extern "C" function
//! in the plug-in shared library as follows:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! The typedef that describes the XRdOssStatInfoInit external.
//------------------------------------------------------------------------------

typedef XrdOss *(*XrdOssGetStorageSystem_t) (XrdOss        *native_oss,
                                             XrdSysLogger  *Logger,
                                             const char    *config_fn,
                                             const char    *parms);

typedef XrdOss *(*XrdOssGetStorageSystem2_t)(XrdOss       *native_oss,
                                             XrdSysLogger *Logger,
                                             const char   *config_fn,
                                             const char   *parms,
                                             XrdOucEnv    *envP);

typedef XrdOssGetStorageSystem2_t XrdOssAddStorageSystem2_t;

/*!
    extern "C" XrdOss *XrdOssGetStorageSystem(XrdOss       *native_oss,
                                              XrdSysLogger *Logger,
                                              const char   *config_fn,
                                              const char   *parms);

    An alternate entry point may be defined in lieu of the previous entry point.
    The plug-in loader looks for this entry point first before reverting to the
    older version 1 entry point/ Version 2 differs in that an extra parameter,
    the environmental pointer, is passed. Note that this pointer is also
    supplied via the EnvInfo() method. This, many times, is not workable as
    environmental information is needed as initialization time.

    extern "C" XrdOss *XrdOssGetStorageSystem2(XrdOss       *native_oss,
                                               XrdSysLogger *Logger,
                                               const char   *config_fn,
                                               const char   *parms,
                                               XrdOucEnv    *envP);

    When pushing additional wrappers, the following entry point is called
    for each library that is stacked. The parameter, curr_oss is the pointer
    to the fully initialized oss plugin being wrapped. The function should
    return a pointer to the wrapping plug-in or nil upon failure.

    extern "C" XrdOss *XrdOssAddStorageSystem2(XrdOss       *curr_oss,
                                               XrdSysLogger *Logger,
                                               const char   *config_fn,
                                               const char   *parms,
                                               XrdOucEnv    *envP);
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
