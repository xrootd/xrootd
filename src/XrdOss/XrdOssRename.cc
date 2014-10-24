/******************************************************************************/
/*                                                                            */
/*                       X r d O s s R e n a m e . c c                        */
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

#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysFAttr.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssCache.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssPath.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdFrc/XrdFrcXAttr.hh"

/******************************************************************************/
/*           G l o b a l   E r r o r   R o u t i n g   O b j e c t            */
/******************************************************************************/

extern XrdSysError OssEroute;

extern XrdOucTrace OssTrace;
  
/******************************************************************************/
/*                                R e n a m e                                 */
/******************************************************************************/

/*
  Function: Renames a file with name 'old_name' to 'new_name'.

  Input:    old_name  - Is the fully qualified name of the file to be renamed.
            new_name  - Is the fully qualified name that the file is to have.
            old_env   - Environmental information for old_name.
            new_env   - Environmental information for new_name.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdOssSys::Rename(const char *oldname, const char *newname,
                      XrdOucEnv  *old_env, XrdOucEnv  *new_env)
{
    EPNAME("Rename")
    static const mode_t pMode = S_IRWXU | S_IRWXG;
    unsigned long long remotefs_Old, remotefs_New, remotefs, haslf;
    unsigned long long old_popts, new_popts;
    int i, retc2, retc = XrdOssOK;
    struct stat statbuff;
    char  *slashPlus, sPChar;
    char  local_path_Old[MAXPATHLEN+8], *lpo;
    char  local_path_New[MAXPATHLEN+8], *lpn;
    char remote_path_Old[MAXPATHLEN+1];
    char remote_path_New[MAXPATHLEN+1];

// Determine whether we can actually rename a file on this server.
//
   remotefs_Old = Check_RO(Rename, old_popts, oldname, "rename");
   remotefs_New = Check_RO(Rename, new_popts, newname, "rename to");

// Make sure we are renaming within compatible file systems
//
   if (remotefs_Old ^ remotefs_New
   || ((old_popts & XRDEXP_MIG) ^ (new_popts & XRDEXP_MIG)))
      {char buff[MAXPATHLEN+128];
       snprintf(buff, sizeof(buff), "rename %s to ", oldname);
       return OssEroute.Emsg("Rename",-XRDOSS_E8011,buff,(char *)newname);
      }
   remotefs = remotefs_Old | remotefs_New;
   haslf    = (XRDEXP_MAKELF & (old_popts | new_popts));

// Construct the filename that we will be dealing with.
//
   if ( (retc = GenLocalPath( oldname, local_path_Old))
     || (retc = GenLocalPath( newname, local_path_New)) ) return retc;
   if (remotefs
     && (((retc = GenRemotePath(oldname, remote_path_Old))
     ||   (retc = GenRemotePath(newname, remote_path_New)))) ) return retc;

// Make sure that the target file does not exist
//
   retc2 = lstat(local_path_New, &statbuff);
   if (!retc2) return -EEXIST;

// We need to create the directory path if it does not exist.
//
   if (!(slashPlus = rindex(local_path_New, '/'))) return -EINVAL;
   slashPlus++; sPChar = *slashPlus; *slashPlus = '\0';
   retc2 = XrdOucUtils::makePath(local_path_New, pMode);
   *slashPlus = sPChar;
   if (retc2) return retc2;

// Check if this path is really a symbolic link elsewhere
//
    if (lstat(local_path_Old, &statbuff)) retc = -errno;
       else if ((statbuff.st_mode & S_IFMT) == S_IFLNK)
               retc = RenameLink(local_path_Old, local_path_New);
               else if (rename(local_path_Old, local_path_New)) retc = -errno;
    DEBUG("lcl rc=" <<retc <<" op=" <<local_path_Old <<" np=" <<local_path_New);

// For migratable space, rename all suffix variations of the base file
//
   if (haslf && runOld)
      {if ((!retc || retc == -ENOENT))
          {i = strlen(local_path_Old); lpo = &local_path_Old[i];
           i = strlen(local_path_New); lpn = &local_path_New[i];
           for (i = 0;  i < XrdOssPath::sfxMigL; i++)
               {strcpy(lpo,XrdOssPath::Sfx[i]); strcpy(lpn,XrdOssPath::Sfx[i]);
                if (rename(local_path_Old,local_path_New) && ENOENT != errno)
                   DEBUG("sfx retc=" <<errno <<" op=" <<local_path_Old);
               }
          }
      }

// Now rename the data file in the remote system if the local rename "worked".
// Do not do this if we really should not use the MSS.
//
   if (remotefs)
      {if (remotefs && (!retc || retc == -ENOENT) && RSSCmd)
          {if ( (retc2 = MSS_Rename(remote_path_Old, remote_path_New))
              != -ENOENT) retc = retc2;
           DEBUG("rmt rc=" <<retc2 <<" op=" <<remote_path_Old <<" np=" <<remote_path_New);
          }
      }

// All done.
//
   return retc;
}
 
/******************************************************************************/
/*                       p r i v a t e   m e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                            R e n a m e L i n k                             */
/******************************************************************************/

int XrdOssSys::RenameLink(char *old_path, char *new_path)
{
   struct stat statbuff;
   char oldlnk[MAXPATHLEN+32], newlnk[MAXPATHLEN+32];
   int lnklen, n, rc = 0;

// Read the contents of the link
//
   if ((lnklen = readlink(old_path,oldlnk,sizeof(oldlnk)-1)) < 0) return -errno;
   oldlnk[lnklen] = '\0';

// Check if this is new or old style cache. Check if this is an offline rename
// and if so, add the space to the usage to account for stage-ins
//
   if (oldlnk[lnklen-1] == XrdOssPath::xChar)
      {rc = (runOld ? RenameLink2(lnklen,oldlnk,old_path,newlnk,new_path)
                    : RenameLink3(oldlnk, old_path, new_path));
       if (rc) return rc;
       if (Solitary && UDir)
          {n = strlen(old_path);
           if (n < 6 || strcmp(old_path+n-5, ".anew")
           ||  stat(new_path, &statbuff) || !statbuff.st_size) return 0;
           XrdOssPath::Trim2Base(oldlnk+lnklen-1);
           XrdOssCache::Adjust(oldlnk, statbuff.st_size);
          }
       return 0;
      }

// Convert old name to the new name
//
   if ((rc = XrdOssPath::Convert(newlnk, sizeof(newlnk), oldlnk, new_path)))
      {OssEroute.Emsg("RenameLink", rc, "convert", oldlnk);
       return rc;
      }

// Make sure that the target name does not exist
//
   if (!lstat(newlnk, &statbuff))
      {OssEroute.Emsg("RenameLink",-EEXIST,"check new target", newlnk);
       return -EEXIST;
      }

// Insert a new link in the target cache
//
   if (symlink(newlnk, new_path))
      {rc = errno;
       OssEroute.Emsg("RenameLink", rc, "symlink to", newlnk);
       return -rc;
      }

// Rename the actual target file
//
   if (rename(oldlnk, newlnk))
      {rc = errno;
       OssEroute.Emsg("RenameLink", rc, "rename", oldlnk);
       unlink(new_path);
       return -rc;
      }

// Now, unlink the source path
//
   if (unlink(old_path))
      OssEroute.Emsg("RenameLink", rc, "unlink", old_path);

// All done
//
   return 0;
}

/******************************************************************************/
/*                           R e n a m e L i n k 2                            */
/******************************************************************************/
  
int XrdOssSys::RenameLink2(int Llen, char *oLnk, char *old_path,
                                     char *nLnk, char *new_path)
{
   int rc;

// Setup to create new pfn file for this file
//
   strcpy(nLnk, oLnk);
   strcpy(nLnk+Llen, ".pfn");
   unlink(nLnk);

// Create the new pfn symlink
//
   if (symlink(new_path, nLnk))
      {rc = errno;
       OssEroute.Emsg("RenameLink", rc, "create symlink", nLnk);
       return -rc;
      }

// Create the new lfn symlink
//
   if (symlink(oLnk, new_path))
      {rc = errno;
       OssEroute.Emsg("RenameLink", rc, "symlink to", oLnk);
       unlink(nLnk);
       return -rc;
      }

// Now, unlink the old lfn path
//
   if (unlink(old_path))
      OssEroute.Emsg("RenameLink", errno, "unlink", old_path);

// All done (well, as well as it needs to be at this point)
//
   return 0;
}

/******************************************************************************/
/*                           R e n a m e L i n k 3                            */
/******************************************************************************/
  
int XrdOssSys::RenameLink3(char *cPath, char *old_path, char *new_path)
{
   int rc;
  
// First set the new extended attribute on this file
//
   if ((rc = XrdSysFAttr::Xat->Set(XrdFrcXAttrPfn::Name(), new_path,
                                   strlen(new_path)+1, cPath))) return rc;

// Now merely rename the old to the new
//
   if (!rename(old_path, new_path)) return 0;

// Rename failed, restore old attribute
//
   rc = -errno;
   XrdSysFAttr::Xat->Set(XrdFrcXAttrPfn::Name(),old_path,strlen(old_path)+1,cPath);
   OssEroute.Emsg("RenameLink", rc, "rename", old_path);
   return rc;
}
