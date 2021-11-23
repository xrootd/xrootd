#ifndef __XRDOSS_CACHE_H__
#define __XRDOSS_CACHE_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O s s C a c h e . h h                         */
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

#include <ctime>
#include <sys/stat.h>
#include "XrdOuc/XrdOucDLlist.hh"
#include "XrdOss/XrdOssVS.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*              O S   D e p e n d e n t   D e f i n i t i o n s               */
/******************************************************************************/

#ifdef __solaris__
#include <sys/statvfs.h>
#define STATFS_t struct statvfs
#define FS_Stat(a,b) statvfs(a,b)
#define FS_BLKSZ f_frsize
#define FS_FFREE f_favail
#endif
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
#include <sys/vfs.h>
#define FS_Stat(a,b) statfs(a,b)
#define STATFS_t struct statfs
#define FS_BLKSZ f_bsize
#define FS_FFREE f_ffree
#endif
#ifdef AIX
#include <sys/statfs.h>
#define STATFS_t struct statfs
#define FS_Stat(a,b) statfs(a,b)
#define FS_BLKSZ f_bsize
#define FS_FFREE f_ffree
#endif
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#define STATFS_t struct statfs
#define FS_Stat(a,b) statfs(a,b)
#define FS_BLKSZ f_bsize
#define FS_FFREE f_ffree
#endif

/******************************************************************************/
/*                     X r d O s s C a c h e _ S p a c e                      */
/******************************************************************************/

class XrdOssCache_Space
{
public:

long long          Total;
long long          Free;
long long          Maxfree;
long long          Largest;
long long          Inodes;
long long          Inleft;
long long          Usage;
long long          Quota;

     XrdOssCache_Space() : Total(0), Free(0), Maxfree(0), Largest(0),
                           Inodes(0), Inleft(0), Usage(-1), Quota(-1) {}
    ~XrdOssCache_Space() {}
};
  
/******************************************************************************/
/*                    X r d O s s C a c h e _ F S D a t a                     */
/******************************************************************************/
  
// Flags values for FSData
//
#define XrdOssFSData_OFFLINE  0x0001
#define XrdOssFSData_ADJUSTED 0x0002
#define XrdOssFSData_REFRESH  0x0004

class XrdOssCache_FSData
{
public:

XrdOssCache_FSData *next;
long long           size;
long long           frsz;
dev_t               fsid;
const char         *path;
const char         *pact;
const char         *devN;
time_t              updt;
int                 stat;
unsigned short      bdevID;
unsigned short      partID;

       XrdOssCache_FSData(const char *, STATFS_t &, dev_t);
      ~XrdOssCache_FSData() {if (path) free((void *)path);}
};

/******************************************************************************/
/*                        X r d O s s C a c h e _ F S                         */
/******************************************************************************/

class XrdOssCache_Group;
  
class XrdOssCache_FS
{
public:

enum FSOpts {None = 0, isXA = 1};

XrdOssCache_FS     *next;
const   char       *group;
const   char       *path;
int                 plen;
FSOpts              opts;
        char        suffix[4];  // Corresponds to OssPath::sfxLen
XrdOssCache_FSData *fsdata;
XrdOssCache_Group  *fsgroup;

static int          Add(const char *Path);
static long long    freeSpace(long long         &Size,  const char *path=0);
static long long    freeSpace(XrdOssCache_Space &Space, const char *path);

static int          getSpace( XrdOssCache_Space &Space, const char *sname,
                              XrdOssVSPart **vsPart=0);
static int          getSpace( XrdOssCache_Space &Space, XrdOssCache_Group *fsg,
                              XrdOssVSPart **vsPart=0);

       XrdOssCache_FS(      int  &retc,
                      const char *fsg,
                      const char *fsp,
                      FSOpts      opt);
      ~XrdOssCache_FS() {if (group) free((void *)group);
                         if (path)  free((void *)path);
                        }
};

/******************************************************************************/
/*                      X r d O s s C a c h e _ F S A P                       */
/******************************************************************************/
  
struct XrdOssCache_FSAP
{
XrdOssCache_FSData  *fsP;   // Pointer to partition
const char         **apVec; // Allocation root paths in this partition (nil end)
int                  apNum;
};

/******************************************************************************/
/*                     X r d O s s C a c h e _ G r o u p                      */
/******************************************************************************/
  
// Eventually we will have management information associated with cache groups
//
class XrdOssCache_Group
{
public:

XrdOssCache_Group   *next;
char                *group;
XrdOssCache_FS      *curr;
XrdOssCache_FSAP    *fsVec; // Partitions where space may be allocated
long long            Usage;
long long            Quota;
int                  GRPid;
short                fsNum;
short                rsvd;
static
XrdOssCache_Group   *PubGroup;
static long long     PubQuota;

static XrdOssCache_Group *fsgroups;

       XrdOssCache_Group(const char *grp, XrdOssCache_FS *fsp=0) 
                        : next(0), group(strdup(grp)), curr(fsp), fsVec(0),
                          Usage(0), Quota(-1), GRPid(-1), fsNum(0), rsvd(0)
                        {if (!strcmp("public", grp)) PubGroup = this;}
      ~XrdOssCache_Group() {if (group) free((void *)group);}
};
  
/******************************************************************************/
/*                           X r d O s s C a c h e                            */
/******************************************************************************/

class XrdOssCache
{
public:

static void            Adjust(dev_t devid, off_t size);

static void            Adjust(const char *Path, off_t size, struct stat *buf=0);

static void            Adjust(XrdOssCache_FS *fsp, off_t size);

struct allocInfo
      {const char     *Path;     // Req: Local file  name
       const char     *cgName;   // Req: Cache group name
       long long       cgSize;   // Opt: Estimated size
       const char     *cgPath;   // Opt: Specific  partition path
       int             cgPlen;   // Opt: Length of partition path
       int             cgPFsz;   // Req: Size of buffer
       char           *cgPFbf;   // Req: Buffer for cache pfn of size cgPFsz
       char           *cgPsfx;   // Out: -> pfn suffix area. If 0, non-xa cache
       XrdOssCache_FS *cgFSp;    // Out: -> Cache file system definition
       mode_t          aMode;    // Opt: Create mode; if 0, pfn file not created

       allocInfo(const char *pP, char *bP, int bL)
                : Path(pP),   cgName(0), cgSize(0), cgPath(0), cgPlen(0),
                  cgPFsz(bL), cgPFbf(bP), cgPsfx(0), cgFSp(0), aMode(0) {}
      ~allocInfo() {}
      };

static int             Alloc(allocInfo &aInfo);

static void            DevInfo(struct stat &buf, bool limits=false);

static XrdOssCache_FS *Find(const char *Path, int lklen=0);

static int             Init(const char *UDir, const char *Qfile,
                            int isSOL, int usync=0);

static int             Init(long long aMin, int ovhd, int aFuzz);

static void            List(const char *lname, XrdSysError &Eroute);

static void            MapDevs(bool dBug=false);

static char           *Parse(const char *token, char *cbuff, int cblen);

static void           *Scan(int cscanint);

                       XrdOssCache() {}
                      ~XrdOssCache() {}

static XrdSysMutex         Mutex;    // Cache context lock

static long long           fsTotal;  // Total number of bytes known
static long long           fsLarge;  // Total number of bytes in largest fspart
static long long           fsTotFr;  // Total number of bytes free
static long long           fsFree;   // Maximum contiguous free space
static long long           fsSize;   // Size of partition with fsFree
static XrdOssCache_FS     *fsfirst;  // -> First  filesystem
static XrdOssCache_FS     *fslast;   // -> Last   filesystem
static XrdOssCache_FSData *fsdata;   // -> Filesystem data
static int                 fsCount;  // Number of file systems

private:
static bool MapDM(const char *ldm, char *buff, int blen);

static long long           minAlloc;
static double              fuzAlloc;
static int                 ovhAlloc;
static int                 Quotas;
static int                 Usage;
};
#endif
