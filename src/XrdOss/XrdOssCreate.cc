/******************************************************************************/
/*                                                                            */
/*                       X r d O s s C r e a t e . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

const char *XrdOssCreateCVSID = "$Id$";

/******************************************************************************/
/*                             i n c l u d e s                                */
/******************************************************************************/
  
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream.h>
#include <strings.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(SUNCC) || defined(AIX)
#include <sys/vnode.h>
#endif

#include "Experiment/Experiment.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssLock.hh"
#include "XrdOss/XrdOssOpaque.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucError.hh"

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/
  
extern XrdOucError OssEroute;

extern XrdOucTrace OssTrace;

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOssSys XrdOssSS;

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

  Output:   Returns XRDOSS_OK upon success; (-errno) otherwise.
*/
int XrdOssSys::Create(const char *path, mode_t access_mode, XrdOucEnv &env)
{
    const char *epname = "Create";
    const int LKFlags = XrdOssFILE|XrdOssSHR|XrdOssNOWAIT|XrdOssRETIME;
    char  local_path[XrdOssMAX_PATH_LEN+1];
    char remote_path[XrdOssMAX_PATH_LEN+1];
    int popts, retc, slen, remotefs, datfd;
    XrdOssLock path_dir, new_file;

// Determine whether we can actually create a file on this server.
//
   remotefs = Check_RO(Create, popts, (char *)path, "creating ");

// Generate the actual local path for this file.
//
   if (!remotefs) strcpy(local_path, path);
      else if (retc=XrdOssSS.GenLocalPath(path, local_path)) return retc;

// If this is a staging filesystem then we have lots more work to do.
//
   if (remotefs)
      {
      // Generate the remote path for this file
      //
         if (retc=XrdOssSS.GenRemotePath(path,remote_path)) return retc;

      // Gain exclusive control over the directory.
      //
         if ( (retc = path_dir.Serialize(local_path, XrdOssDIR|XrdOssEXC)) < 0)
            return retc;

     // Create the file in remote system unless not wanted so
     //
        if (popts & XrdOssRCREATE)
           {if ((retc = MSS_Create(remote_path, access_mode, env)) < 0)
               {path_dir.UnSerialize(0);
                DEBUG("rc" <<retc <<" mode=" <<std::oct <<access_mode
                           <<std::dec <<" remote path=" <<remote_path);
                return retc;
               }
           } else if (!(popts & XrdOssNOCHECK))
                     {struct stat fstat;
                      if (!(retc = MSS_Stat(remote_path, &fstat)))
                         {path_dir.UnSerialize(0); return -EEXIST;}
                         else if (retc != -ENOENT)
                                 {path_dir.UnSerialize(0); return retc;}
                     }
      }

// If extended cache is to be used, call out the cache routine
//
   if (fsfirst && !(popts & XrdOssINPLACE))
           datfd = Alloc_Cache((const char *)local_path, access_mode, env);
      else datfd = Alloc_Local((const char *)local_path, access_mode, env);

// Diagnose file creation problems at this point
//
   if (datfd < 0) retc = datfd;
      else retc = XrdOssOK;

// If successful, appropriately manage the locks.
//
   if (datfd >= 0)
      {if (remotefs)
          {if ((new_file.Serialize(local_path,LKFlags))
                >= 0) new_file.UnSerialize(0);
           path_dir.UnSerialize(0);
          }
       close(datfd);
      }

// All done.
//
   return retc;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                           A l l o c _ C a c h e                            */
/******************************************************************************/

int XrdOssSys::Alloc_Cache(const char *path, mode_t amode, XrdOucEnv &env)
{
   const char *epname = "Alloc_Cache";
   double fuzz, diffree;
   int datfd, rc;
   XrdOssCache_FS *fsp, *fspend, *fsp_sel;
   XrdOssCache_Group *cgp = 0;
   XrdOssCache_Lock Dummy; // Obtains & releases the lock
   long long size, maxfree, curfree;
   char pbuff[XrdOssMAX_PATH_LEN+1], *pbp, *pap, *cgroup, *vardata;

// Grab the suggested size from the environment
//
   if (!(vardata = env.Get(OSS_ASIZE))) size = 0;
      else if (!XrdOuca2x::a2ll(OssEroute,"invalid asize",vardata,&size,0))
              return -XRDOSS_E8018;

// Get the correct cache group
//
   if (!(cgroup = env.Get(OSS_CGROUP))) cgroup = OSS_CGROUP_DEFAULT;

// Compute appropriate allocation size
//
   if ( (size = size * ovhalloc / 100 + size) < minalloc)
      size = minalloc;

// Select correct cursor and fuzz amount
//
   cgp = fsgroups;
   while(cgp && strcmp(cgroup, cgp->group)) cgp = cgp->next;
   if (!cgp) return -XRDOSS_E8019;
   fsp = cgp->curr;
   fuzz = ((double)fuzalloc)/100.0;

// Find a cache that will fit this allocation request
//
   maxfree = fsp->fsdata->frsz; fspend = fsp; fsp_sel = fsp; fsp = fsp->next;
   do {
       if (strcmp(cgroup, fsp->group)) continue;
       curfree = fsp->fsdata->frsz;
       if (size > curfree) continue;

       if (!fuzz) {if (curfree > maxfree) {fsp_sel = fsp; maxfree = curfree;}}
          else {if (!(curfree + maxfree)) diffree = 0.0;
                   else diffree = (double)(curfree - maxfree)/
                                  (double)(curfree + maxfree);
                if (diffree > fuzz) {fsp_sel = fsp; maxfree = curfree;}
               }
      } while((fsp = fsp->next) != fspend);

// Check if we can realy fit this file
//
   if (size > maxfree) return -XRDOSS_E8020;

// Construct the target filename
//
   if ((fsp_sel->fsdata->plen + strlen(path)) >= sizeof(pbuff))
      return -ENAMETOOLONG;
   strcpy(pbuff, fsp_sel->path);
   pbp = &pbuff[fsp_sel->fsdata->plen];
   pap = (char *)path;
   XrdOssTAMP(pbp, pap);

// Simply open the file in the local filesystem, creating it if need be.
//
   do {datfd = open(pbuff, O_RDWR|O_CREAT|O_EXCL|O_LARGEFILE, amode);}
               while(datfd < 0 && errno == EINTR);

// Now create a symbolic link to the target and adjust free space
//
   if (datfd < 0) datfd = -errno;
      else if (symlink(pbuff, path))
              {rc = -errno; close(datfd); unlink(pbuff); datfd = rc;}
              else fsp_sel->fsdata->frsz -= size;

// Update the cursor address
//
   if (cgp) cgp->curr = fsp_sel->next;
      else fscurr = fsp_sel->next;

// All done
//
   DEBUG(cgroup <<" cache as " <<pbuff);
   return datfd;
}

/******************************************************************************/
/*                           A l l o c _ L o c a l                            */
/******************************************************************************/
  
int XrdOssSys::Alloc_Local(const char *path, mode_t amode, XrdOucEnv &env)
{
   int datfd;

// Simply open the file in the local filesystem, creating it if need be.
//
   do {datfd = open(path, O_RDWR|O_CREAT|O_EXCL|O_LARGEFILE, amode);}
               while(datfd < 0 && errno == EINTR);
   return (datfd < 0 ? -errno : datfd);
}
