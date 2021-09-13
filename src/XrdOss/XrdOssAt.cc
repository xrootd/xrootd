/******************************************************************************/
/*                                                                            */
/*                           X r d O s s A t . c c                            */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/param.h>
#include <sys/stat.h>

#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssAt.hh"
#include "XrdOss/XrdOssCache.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssPath.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                      E x t e r n a l   O b j e c t s                       */
/******************************************************************************/

extern XrdSysError OssEroute;
  
/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/
  
// Common prologue fo each public method
//
#ifdef HAVE_FSTATAT
#define BOILER_PLATE(fd, dfObj) \
       {if (!(dfObj.DFType() & XrdOssDF::DF_isDir))  return -ENOTDIR;\
        if (!path || *path == '/') return -XRDOSS_E8027;\
        if ((fd = dfObj.getFD()) < 0) return -XRDOSS_E8002;\
       }
#else
#define BOILER_PLATE(dfObj,fd) return -ENOTSUP;
#endif

// Open the target
//
#ifdef O_CLOEXEC
#define OPEN_AT(dst, dfd, p, f)\
   dst = openat(dfd, p, f|O_CLOEXEC);\
   if (dst < 0) return -errno;
#else
#define OPEN_AT(dst, dfd, p, f)\
   dst = openat(dfd, p, f); \
  if (dst >= 0) fcntl(dst, F_SETFD, FD_CLOEXEC);\
     else return -errno
#endif

namespace
{
class openHelper
     {public:
      int  FD;
      openHelper() : FD(-1) {}
     ~openHelper() {if (FD >= 0) close(FD);}
     };
}

/******************************************************************************/
/*                               O p e n d i r                                */
/******************************************************************************/
  
int XrdOssAt::Opendir(XrdOssDF  &atDir, const char *path, XrdOucEnv &env,
                      XrdOssDF *&ossDF)
{
   openHelper hOpen;
   DIR       *dirP;
   int dirFD;

// Standard boilerplate
//
   BOILER_PLATE(dirFD, atDir);

// Open the target
//
   OPEN_AT(hOpen.FD, dirFD, path, O_RDONLY);

// Create a new dir entry from this FD
//
   dirP = fdopendir(hOpen.FD);
   if (!dirP) return (errno ? -errno : -ENOMSG);

// Finally return a new directory object
//
   ossDF = new XrdOssDir(atDir.getTID(), dirP);
   hOpen.FD = -1;
   return 0;
}

/******************************************************************************/
/*                                O p e n R O                                 */
/******************************************************************************/
  
int XrdOssAt::OpenRO(XrdOssDF  &atDir, const char *path, XrdOucEnv &env,
                     XrdOssDF *&ossDF)
{
   openHelper  hOpen;
   int dirFD;

// Standard boilerplate
//
   BOILER_PLATE(dirFD, atDir);

// Open the target
//
   OPEN_AT(hOpen.FD, dirFD, path, O_RDONLY);

// Return a new file object
//
   ossDF = new XrdOssFile(atDir.getTID(), hOpen.FD);
   hOpen.FD = -1;
   return 0;
}

/******************************************************************************/
/*                                R e m d i r                                 */
/******************************************************************************/
  
int XrdOssAt::Remdir(XrdOssDF  &atDir, const char *path)
{
   int dirFD;

// Standard boilerplate
//
   BOILER_PLATE(dirFD, atDir);

// Effect the removal
//
   if (unlinkat(dirFD, path, AT_REMOVEDIR)) return -errno;

// All done
//
   return 0;
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/
  
int XrdOssAt::Stat(XrdOssDF &atDir, const char *path, struct stat &buf,
                   int opts)
{
   int dirFD;

// Standard boilerplate
//
   BOILER_PLATE(dirFD, atDir);

// Do the stat call
//
   if (fstatat(dirFD, path, &buf, 0)) return -errno;

// Check if we need to provide dev info
//
   if (opts & At_dInfo) XrdOssCache::DevInfo(buf);

// All done
//
   return 0;
}

/******************************************************************************/
/*                                U n l i n k                                 */
/******************************************************************************/
  
int XrdOssAt::Unlink(XrdOssDF  &atDir, const char *path)
{
   struct stat Stat;
   int dirFD;

// Standard boilerplate
//
   BOILER_PLATE(dirFD, atDir);

// This could be a symlink or an actual file but not a directory.
//
   if (fstatat(dirFD, path, &Stat, AT_SYMLINK_NOFOLLOW))
      return (errno == ENOENT ? 0 : -errno);
   if ((Stat.st_mode & S_IFMT) == S_IFDIR) return -EISDIR;

// If this is not a symlink then we can delete it directly
//
   if ((Stat.st_mode & S_IFMT) != S_IFLNK)
      {if (unlinkat(dirFD, path, 0)) return -errno;
       if (Stat.st_size)
          XrdOssCache::Adjust(Stat.st_dev, -Stat.st_size);
       return 0;
      }

// Get the target of this link
//
   char lnkbuff[MAXPATHLEN+64];
   int lnklen, retc;
   if ((lnklen = readlinkat(dirFD, path, lnkbuff, sizeof(lnkbuff)-1)) < 0)
      return -errno;

// If the underlying file exists, remove it
//
   lnkbuff[lnklen] = '\0';
   if (stat(lnkbuff, &Stat)) Stat.st_size = 0;
      else if (unlink(lnkbuff) && errno != ENOENT)
              {retc = -errno;
               OssEroute.Emsg("Unlink",retc,"unlink symlink target",lnkbuff);
               return -retc;
              }

// Adjust the size based on what kind of data cache we are using.
//
   if (Stat.st_size)
      {char *lP = lnkbuff+lnklen-1;
       if (*lP == XrdOssPath::xChar)
          {XrdOssPath::Trim2Base(lP);
           XrdOssCache::Adjust(lnkbuff, -Stat.st_size);
          }
          else XrdOssCache::Adjust(Stat.st_dev, -Stat.st_size);
      }

// Effect the removal of the actual symlink
//
   if (unlinkat(dirFD, path, 0)) return -errno;

// All done
//
   return 0;
}
