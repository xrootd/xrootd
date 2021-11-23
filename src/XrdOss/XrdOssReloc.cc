/******************************************************************************/
/*                                                                            */
/*                        X r d O s s R e l o c . c c                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/******************************************************************************/
/*                             i n c l u d e s                                */
/******************************************************************************/
  
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <strings.h>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>

#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssCache.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssCopy.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssPath.hh"
#include "XrdOss/XrdOssSpace.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/
  
extern XrdSysError OssEroute;

extern XrdSysTrace OssTrace;

extern XrdOssSys  *XrdOssSS;

/******************************************************************************/
/*                                 R e l o c                                  */
/******************************************************************************/

/*
  Function: Relocate/Copy the file at `path' to a new location.

  Input:    path        - The fully qualified name of the file to relocate.
            cgName      - Target space name[:path]
            anchor      - The base path where a symlink to the copied file is
                          to be created. If present, the original file is kept.
                          If anchor is "." then path is taken as pfn (not lfn)
                          and a pure relocation is performed.

  Output:   Returns XrdOssOK upon success; (-errno) otherwise.
*/
int XrdOssSys::Reloc(const char *tident, const char *path,
                     const char *cgName, const char *anchor)
{
   EPNAME("Reloc")
   const int AMode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH; // 775
   class pendFiles
        {public:
         char *pbuff;
         char *tbuff;
         int   datfd;
               pendFiles(char *pb, char *tb) : datfd(-1)
                           {pbuff = pb; *pb = '\0';
                            tbuff = tb; *tb = '\0';
                           }
              ~pendFiles() {if (datfd >= 0) close(datfd);
                            if (pbuff && *pbuff) unlink(pbuff);
                            if (tbuff && *tbuff) unlink(tbuff);
                           }
        };
   char cgNow[XrdOssSpace::minSNbsz], cgbuff[XrdOssSpace::minSNbsz];
   char lbuff[MAXPATHLEN+8];
   char pbuff[MAXPATHLEN+8];
   char tbuff[MAXPATHLEN+8];
   char local_path[MAXPATHLEN+8];
   pendFiles PF(pbuff, tbuff);
   XrdOssCache::allocInfo aInfo(path, pbuff, sizeof(pbuff));
   int rc, lblen, datfd, Pure = (anchor && !strcmp(anchor, "."));
   off_t rc_c;
   struct stat buf;

// Generate the actual local path for this file.
//
   if (Pure) {strcpy(local_path, path); anchor = 0;}
      else if ((rc = GenLocalPath(path, local_path))) return rc;

// Determine the state of the file.
//
   if (stat(local_path, &buf)) return -errno;
   if ((buf.st_mode & S_IFMT) == S_IFDIR) return -EISDIR;
   if ((buf.st_mode & S_IFMT) != S_IFREG) return -ENOTBLK;

// Get the correct cache group and partition path
//
   if ((aInfo.cgPath = XrdOssCache::Parse(cgName, cgbuff, sizeof(cgbuff))))
      aInfo.cgPlen = strlen(aInfo.cgPath);

// Verify that this file will go someplace other than where it is now
//
   lblen = XrdOssPath::getCname(local_path, cgNow, lbuff, sizeof(lbuff)-7);
   lbuff[lblen] = '\0';
   if (!Pure && !strcmp(cgbuff, cgNow)
   && (!aInfo.cgPath || !strncmp(aInfo.cgPath, lbuff, aInfo.cgPlen)))
      return -EEXIST;

// Allocate space in the cache. Note that the target must be an xa cache
//
   aInfo.aMode  = buf.st_mode & S_IAMB;
   aInfo.cgSize = (Pure ? 0 : buf.st_size);
   aInfo.cgName = cgbuff;
   if ((PF.datfd = datfd = XrdOssCache::Alloc(aInfo)) < 0) return datfd;
   if (!aInfo.cgPsfx) return -ENOTSUP;

// Copy the original file to the new location. Copy() always closes the fd.
//
   PF.datfd = -1;
   if ((rc_c = XrdOssCopy::Copy(local_path, pbuff, datfd)) < 0) return (int)rc_c;

// If the file is to be merely copied, substitute the desired destination
//
   if (!anchor) {strcpy(tbuff, local_path); strcat(tbuff, ".anew");}
      else {struct stat sbuf;
            char *Slash;
            if (strlen(anchor)+strlen(path) >= sizeof(local_path))
               return -ENAMETOOLONG;
            strcpy(local_path, anchor); strcat(local_path, path);
            if (!(Slash = rindex(local_path, '/'))) return -ENOTDIR;
            *Slash = '\0'; rc = stat(local_path, &sbuf); *Slash = '/';
            if (rc && (rc = XrdOucUtils::makePath(local_path, AMode)))
               return rc;
            strcpy(tbuff, local_path);
           }

// Now create a symbolic link to the target
//
   if ((symlink(pbuff, tbuff) && errno != EEXIST)
   || unlink(tbuff) || symlink(pbuff, tbuff)) return -errno;

// Rename the link atomically over the existing name
//
   if (!anchor && rename(tbuff, local_path) < 0) return -errno;
   PF.tbuff = 0; PF.pbuff = 0; rc = 0;

// Issue warning if the pfn file could not be created (very very rare).
// At this point we can't do much about it.
//
   if (rc) OssEroute.Emsg("Reloc", rc, "create symlink", pbuff);
   *(aInfo.cgPsfx) = '\0';

// If this was a copy operation, we are done
//
   DEBUG(cgNow <<':' <<local_path <<" -> " <<aInfo.cgName <<':' <<pbuff);
   if (anchor) return XrdOssOK;

// Check if the original file was a symlink and that has to be deleted
// Adjust the space usage numbers at this point as well.
//
   if (*lbuff)
      {if (unlink(lbuff))     OssEroute.Emsg("Reloc",errno,"removing",lbuff);
       XrdOssCache::Adjust(XrdOssCache::Find(lbuff, lblen), -buf.st_size);
       } else XrdOssCache::Adjust(buf.st_dev, -buf.st_size);

// All done (permanently adjust usage for the target)
//
   XrdOssCache::Adjust(aInfo.cgFSp, buf.st_size);
   return XrdOssOK;
}
