/******************************************************************************/
/*                                                                            */
/*                        X r d O s s C a c h e . c c                         */
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
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdOss/XrdOssCache.hh"
#include "XrdOss/XrdOssOpaque.hh"
#include "XrdOss/XrdOssPath.hh"
#include "XrdOss/XrdOssSpace.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
  
/******************************************************************************/
/*            G l o b a l s   a n d   S t a t i c   M e m b e r s             */
/******************************************************************************/

extern XrdSysError  OssEroute;

extern XrdOucTrace  OssTrace;

XrdOssCache_Group  *XrdOssCache_Group::fsgroups = 0;

long long           XrdOssCache_Group::PubQuota = -1;

XrdSysMutex         XrdOssCache::Mutex;
long long           XrdOssCache::fsTotal = 0;
long long           XrdOssCache::fsLarge = 0;
long long           XrdOssCache::fsTotFr = 0;
long long           XrdOssCache::fsFree  = 0;
long long           XrdOssCache::fsSize  = 0;
XrdOssCache_FS     *XrdOssCache::fsfirst = 0;
XrdOssCache_FS     *XrdOssCache::fslast  = 0;
XrdOssCache_FSData *XrdOssCache::fsdata  = 0;
double              XrdOssCache::fuzAlloc= 0.0;
long long           XrdOssCache::minAlloc= 0;
int                 XrdOssCache::fsCount = 0;
int                 XrdOssCache::ovhAlloc= 0;
int                 XrdOssCache::Quotas  = 0;
int                 XrdOssCache::Usage   = 0;

/******************************************************************************/
/*            X r d O s s C a c h e _ F S D a t a   M e t h o d s             */
/******************************************************************************/
  
XrdOssCache_FSData::XrdOssCache_FSData(const char *fsp, 
                                       STATFS_t   &fsbuff,
                                       dev_t       fsID)
{

     path = strdup(fsp);
     size = static_cast<long long>(fsbuff.f_blocks)
          * static_cast<long long>(fsbuff.FS_BLKSZ);
     frsz = static_cast<long long>(fsbuff.f_bavail)
          * static_cast<long long>(fsbuff.FS_BLKSZ);
     XrdOssCache::fsTotal += size;
     XrdOssCache::fsTotFr += frsz;
     XrdOssCache::fsCount++;
     if (size > XrdOssCache::fsLarge) XrdOssCache::fsLarge= size;
     if (frsz > XrdOssCache::fsFree)  XrdOssCache::fsFree = frsz;
     fsid = fsID;
     updt = time(0);
     next = 0;
     stat = 0;
     seen = 0;
}
  
/******************************************************************************/
/*            X r d O s s C a c h e _ F S   C o n s t r u c t o r             */
/******************************************************************************/

// Cache_FS objects are only created during configuration. No locks are needed.
  
XrdOssCache_FS::XrdOssCache_FS(int &retc,
                               const char *fsGrp,
                               const char *fsPath,
                               FSOpts      fsOpts)
{
   static const mode_t theMode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
   STATFS_t fsbuff;
   struct stat sfbuff;
   XrdOssCache_FSData *fdp;
   XrdOssCache_FS     *fsp;
   int n;
   //bool newfs = false;

// Prefill in case of failure
//
   path = group = 0;

// Verify that this is not a duplicate
//
   fsp = XrdOssCache::fsfirst;
   while(fsp && (strcmp(fsp->path,fsPath)||strcmp(fsp->fsgroup->group,fsGrp)))
        if ((fsp = fsp->next) == XrdOssCache::fsfirst) {fsp = 0; break;}
   if (fsp) {retc = EEXIST; return;}

// Set the groupname and the path which is the supplied path/group name
//
   if (!(fsOpts & isXA)) path = strdup(fsPath);
      else {path = XrdOssPath::genPath(fsPath, fsGrp, suffix);
            if (mkdir(path, theMode) && errno != EEXIST) {retc=errno; return;}
           }
   plen   = strlen(path);
   group  = strdup(fsGrp); 
   fsgroup= 0;
   opts   = fsOpts;
   retc   = ENOMEM;

// Find the filesystem for this object
//
   if (FS_Stat(fsPath, &fsbuff) || stat(fsPath, &sfbuff)) {retc=errno; return;}

// Find the matching filesystem data
//
   fdp = XrdOssCache::fsdata;
   while(fdp) {if (fdp->fsid == sfbuff.st_dev) break; fdp = fdp->next;}

// If we didn't find the filesystem, then create one
//
   if (!fdp)
      {if (!(fdp = new XrdOssCache_FSData(fsPath,fsbuff,sfbuff.st_dev))) return;
          else {fdp->next = XrdOssCache::fsdata; XrdOssCache::fsdata = fdp;}
   //    newfs = true;
      }

// Complete the filesystem block (failure now is not an option)
//
   fsdata = fdp;
   retc   = 0;

// Link this filesystem into the filesystem chain
//
   if (!XrdOssCache::fsfirst) {next = this;
                               XrdOssCache::fsfirst = this;
                               XrdOssCache::fslast  = this;
                              }
      else {next = XrdOssCache::fslast->next;
                   XrdOssCache::fslast->next = this;
                   XrdOssCache::fslast       = this;
           }

// Check if this is the first group allocation
//
   fsgroup = XrdOssCache_Group::fsgroups;
   while(fsgroup && strcmp(group, fsgroup->group)) fsgroup = fsgroup->next;
   if (!fsgroup && (fsgroup = new XrdOssCache_Group(group, this)))
      {fsgroup->next = XrdOssCache_Group::fsgroups; 
       XrdOssCache_Group::fsgroups=fsgroup;
      }

// Add a filesystem to this group but only if it is a new one
//
   XrdOssCache_FSAP *fsAP;
   for (n = 0; n < fsgroup->fsNum && fdp != fsgroup->fsVec[n].fsP; n++);
   if (n >= fsgroup->fsNum)
      {n= (fsgroup->fsNum + 1) * sizeof(XrdOssCache_FSAP);
       fsgroup->fsVec = (XrdOssCache_FSAP *)realloc((void *)fsgroup->fsVec,n);
       fsAP = &(fsgroup->fsVec[fsgroup->fsNum++]);
       fsAP->fsP   = fdp;
       fsAP->apVec = 0;
       fsAP->apNum = 0;
      } else fsAP = &(fsgroup->fsVec[n]);

// Add the allocation path to this partition
//
   n = (fsAP->apNum + 2) * sizeof(char *);
   fsAP->apVec = (const char **)realloc((void *)fsAP->apVec, n);
   fsAP->apVec[fsAP->apNum++] = path;
   fsAP->apVec[fsAP->apNum]   = 0;
}

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/

// Add is only called during configuration. No locks are needed. It merely
// adds an unnamed file system partition. This allows us to track space.
  
int XrdOssCache_FS::Add(const char *fsPath)
{
   STATFS_t fsbuff;
   struct stat sfbuff;
   XrdOssCache_FSData *fdp;

// Find the filesystem for this object
//
   if (FS_Stat(fsPath, &fsbuff) || stat(fsPath, &sfbuff)) return -errno;

// Find the matching filesystem data
//
   fdp = XrdOssCache::fsdata;
   while(fdp) {if (fdp->fsid == sfbuff.st_dev) break; fdp = fdp->next;}
   if (fdp) return 0;

// Create new filesystem data that will not be linked to any filesystem
//
   if (!(fdp = new XrdOssCache_FSData(fsPath,fsbuff,sfbuff.st_dev)))
      return -ENOMEM;
   fdp->next = XrdOssCache::fsdata;
   XrdOssCache::fsdata = fdp;
   return 0;
}
  
/******************************************************************************/
/*                             f r e e S p a c e                              */
/******************************************************************************/

long long XrdOssCache_FS::freeSpace(long long &Size, const char *path)
{
   STATFS_t fsbuff;
   long long fSpace;

// Free space for a specific path
//
   if (path)
      {if (FS_Stat(path, &fsbuff)) return -1;
       Size = static_cast<long long>(fsbuff.f_blocks)
            * static_cast<long long>(fsbuff.FS_BLKSZ);
       return static_cast<long long>(fsbuff.f_bavail)
            * static_cast<long long>(fsbuff.FS_BLKSZ);
      }

// Free space for the whole system
//
   XrdOssCache::Mutex.Lock();
   fSpace = XrdOssCache::fsFree;
   Size   = XrdOssCache::fsSize;
   XrdOssCache::Mutex.UnLock();
   return fSpace;
}

/******************************************************************************/

long long XrdOssCache_FS::freeSpace(XrdOssCache_Space &Space, const char *path)
{
   STATFS_t fsbuff;

// Free space for a specific path
//
   if (!path || FS_Stat(path, &fsbuff)) return -1;

   Space.Total = static_cast<long long>(fsbuff.f_blocks)
               * static_cast<long long>(fsbuff.FS_BLKSZ);
   Space.Free  = static_cast<long long>(fsbuff.f_bavail)
               * static_cast<long long>(fsbuff.FS_BLKSZ);
   Space.Inodes= static_cast<long long>(fsbuff.f_files);
   Space.Inleft= static_cast<long long>(fsbuff.FS_FFREE);

   return Space.Free;
}

/******************************************************************************/
/*                              g e t S p a c e                               */
/******************************************************************************/
  
int XrdOssCache_FS::getSpace(XrdOssCache_Space &Space, const char *sname,
                             XrdOssVSPart **vsPart)
{
   XrdOssCache_Group  *fsg = XrdOssCache_Group::fsgroups;

// Try to find the space group name
//
   while(fsg && strcmp(sname, fsg->group)) fsg = fsg->next;
   if (!fsg) return 0;

// Return the accumulated result
//
   return getSpace(Space, fsg, vsPart);
}

/******************************************************************************/

int  XrdOssCache_FS::getSpace(XrdOssCache_Space &Space, XrdOssCache_Group *fsg,
                              XrdOssVSPart **vsPart)
{
   XrdOssVSPart       *pVec;
   XrdOssCache_FSData *fsd;

// Get values that don't change
//
   Space.Usage = fsg->Usage;
   Space.Quota = fsg->Quota;

// If there is no partition table then we really have no space to report
//
   if (fsg->fsNum < 1 || !fsg->fsVec) return 0;

// If partion information is wanted, allocate a partition table
//
   if (vsPart) *vsPart = pVec = new XrdOssVSPart[fsg->fsNum];
      else pVec = 0;

// Prepare to accumulate the stats.
//
   XrdOssCache::Mutex.Lock();
   for (int i = 0; i < fsg->fsNum; i++)
       {fsd = fsg->fsVec[i].fsP;
        Space.Total +=  fsd->size;
        Space.Free  +=  fsd->frsz;
        if (fsd->frsz > Space.Maxfree) Space.Maxfree = fsd->frsz;
        if (fsd->size > Space.Largest) Space.Largest = fsd->size;

        if (pVec)
           {pVec[i].pPath = fsd->path;
            pVec[i].aPath = fsg->fsVec[i].apVec;
            pVec[i].Total = fsd->size;
            pVec[i].Free  = fsd->frsz;
           }
       }
   XrdOssCache::Mutex.UnLock();

// All done
//
   return fsg->fsNum;
}

/******************************************************************************/
/*                                A d j u s t                                 */
/******************************************************************************/
  
void  XrdOssCache::Adjust(dev_t devid, off_t size)
{
   EPNAME("Adjust")
   XrdOssCache_FSData *fsdp;
   XrdOssCache_Group  *fsgp;

// Search for matching filesystem
//
   fsdp = XrdOssCache::fsdata;
   while(fsdp && fsdp->fsid != devid) fsdp = fsdp->next;
   if (!fsdp) {DEBUG("dev " <<devid <<" not found."); return;}

// Find the public cache group (it might not exist)
//
   fsgp = XrdOssCache_Group::fsgroups;
   while(fsgp && strcmp("public", fsgp->group)) fsgp = fsgp->next;

// Process the result
//
   if (fsdp) 
      {DEBUG("free=" <<fsdp->frsz <<'-' <<size <<" path=" <<fsdp->path);
       Mutex.Lock();
       if (        (fsdp->frsz  -= size) < 0) fsdp->frsz = 0;
       fsdp->stat |= XrdOssFSData_ADJUSTED;
       if (fsgp && (fsgp->Usage += size) < 0) fsgp->Usage = 0;
       Mutex.UnLock();
      } else {
       DEBUG("dev " <<devid <<" not found.");
      }
}

/******************************************************************************/
  
void XrdOssCache::Adjust(const char *Path, off_t size, struct stat *buf)
{
   EPNAME("Adjust")
   XrdOssCache_FS *fsp;

// If we have a struct then we need to do some more work
//
   if (buf) 
      {if ((buf->st_mode & S_IFMT) != S_IFLNK) Adjust(buf->st_dev, size);
          else {char lnkbuff[MAXPATHLEN+64];
                int  lnklen = readlink(Path, lnkbuff, sizeof(lnkbuff)-1);
                if (lnklen > 0)
                   {XrdOssPath::Trim2Base(lnkbuff+lnklen-1);
                    Adjust(lnkbuff, size);
                   }
               }
       return;
      }

// Search for matching logical partition
//
   fsp = fsfirst;
   while(fsp && strcmp(fsp->path, Path)) 
        if ((fsp = fsp->next) == fsfirst) {fsp = 0; break;}

// Process the result
//
   if (fsp) Adjust(fsp, size);
      else {DEBUG("cahe path " <<Path <<" not found.");}
}

/******************************************************************************/
  
void XrdOssCache::Adjust(XrdOssCache_FS *fsp, off_t size)
{
   EPNAME("Adjust")
   XrdOssCache_FSData *fsdp;

// Process the result
//
   if (fsp) 
      {fsdp = fsp->fsdata;
       DEBUG("used=" <<fsp->fsgroup->Usage <<'+' <<size <<" path=" <<fsp->path);
       DEBUG("free=" <<fsdp->frsz <<'-' <<size <<" path=" <<fsdp->path);
       Mutex.Lock();
       if ((fsp->fsgroup->Usage += size) < 0) fsp->fsgroup->Usage = 0;
       if (        (fsdp->frsz  -= size) < 0) fsdp->frsz = 0;
       fsdp->stat |= XrdOssFSData_ADJUSTED;
       if (Usage) XrdOssSpace::Adjust(fsp->fsgroup->GRPid, size);
       Mutex.UnLock();
      }
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

int XrdOssCache::Alloc(XrdOssCache::allocInfo &aInfo)
{
   EPNAME("Alloc");
   static const mode_t theMode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
   XrdSysMutexHelper myMutex(&Mutex);
   double diffree;
   XrdOssPath::fnInfo Info;
   XrdOssCache_FS *fsp, *fspend, *fsp_sel;
   XrdOssCache_Group *cgp = 0;
   long long size, maxfree, curfree;
   int rc, madeDir, datfd = 0;

// Compute appropriate allocation size
//
   if (!aInfo.cgSize
   ||  (size=aInfo.cgSize*ovhAlloc/100+aInfo.cgSize) < minAlloc)
      aInfo.cgSize = size = minAlloc;

// Find the corresponding cache group
//
   cgp = XrdOssCache_Group::fsgroups;
   while(cgp && strcmp(aInfo.cgName, cgp->group)) cgp = cgp->next;
   if (!cgp) return -ENOENT;

// Find a cache that will fit this allocation request. We start with the next
// entry past the last one we selected and go full round looking for a
// compatable entry (enough space and in the right space group).
//
   fsp_sel = 0; maxfree = 0;
   fsp = cgp->curr->next; fspend = fsp; // End when we hit the start again
   do {
       if (strcmp(aInfo.cgName, fsp->group)
       || (aInfo.cgPath && (aInfo.cgPlen > fsp->plen
                        ||  strncmp(aInfo.cgPath,fsp->path,aInfo.cgPlen)))) continue;
       curfree = fsp->fsdata->frsz;
       if (size > curfree) continue;

             if (fuzAlloc > 0.999) {fsp_sel = fsp; break;}
       else  if (!fuzAlloc || !fsp_sel)
                {if (curfree > maxfree) {fsp_sel = fsp; maxfree = curfree;}}
       else {diffree = (!(curfree + maxfree) ? 0.0
                     : static_cast<double>(XRDABS(maxfree - curfree)) /
                       static_cast<double>(       maxfree + curfree));
             if (diffree > fuzAlloc) {fsp_sel = fsp; maxfree = curfree;}
            }
      } while((fsp = fsp->next) != fspend);

// Check if we can realy fit this file. If so, update current scan pointer
//
   if (!fsp_sel) return -ENOSPC;
   cgp->curr = fsp_sel;

// Construct the target filename
//
   Info.Path    = fsp_sel->path;
   Info.Plen    = fsp_sel->plen;
   Info.Sfx     = fsp_sel->suffix;
   aInfo.cgPsfx = XrdOssPath::genPFN(Info, aInfo.cgPFbf, aInfo.cgPFsz,
                  (fsp_sel->opts & XrdOssCache_FS::isXA ? 0 : aInfo.Path));

// Verify that target name was constructed
//
   if (!(*aInfo.cgPFbf)) return -ENAMETOOLONG;

// Simply open the file in the local filesystem, creating it if need be.
//
   if (aInfo.aMode)
      {madeDir = 0;
       do {do {datfd = open(aInfo.cgPFbf,O_CREAT|O_TRUNC|O_WRONLY,aInfo.aMode);}
               while(datfd < 0 && errno == EINTR);
           if (datfd >= 0 || errno != ENOENT || madeDir) break;
           *Info.Slash='\0'; rc=mkdir(aInfo.cgPFbf,theMode); *Info.Slash='/';
           madeDir = 1;
          } while(!rc);
       if (datfd < 0) return (errno ? -errno : -EFAULT);
      }

// All done (temporarily adjust down the free space)x
//
   DEBUG("free=" <<fsp_sel->fsdata->frsz <<'-' <<size <<" path=" 
                 <<fsp_sel->fsdata->path);
   fsp_sel->fsdata->frsz -= size;
   fsp_sel->fsdata->stat |= XrdOssFSData_REFRESH;
   aInfo.cgFSp  = fsp_sel;
   return datfd;
}
  
/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/
  
XrdOssCache_FS *XrdOssCache::Find(const char *Path, int lnklen)
{
   XrdOssCache_FS *fsp;
   char lnkbuff[MAXPATHLEN+64];
   struct stat sfbuff;

// First see if this is a symlink that refers to a new style cache
//
   if (lnklen) 
      {if (strlcpy(lnkbuff,Path,sizeof(lnkbuff)) >= sizeof(lnkbuff)) return 0;}
      else if (lstat(Path, &sfbuff)
           ||  (sfbuff.st_mode & S_IFMT) != S_IFLNK
           ||  (lnklen = readlink(Path,lnkbuff,sizeof(lnkbuff)-1)) <= 0)
              return 0;

// Trim the link to the base name
//
   XrdOssPath::Trim2Base(lnkbuff+lnklen-1);

// Search for matching logical partition
//
   fsp = fsfirst;
   while(fsp && strcmp(fsp->path, lnkbuff))
        if ((fsp = fsp->next) == fsfirst) {fsp = 0; break;}
   return fsp;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

// Init() is only called during configuration and no locks are needed.
  
int XrdOssCache::Init(const char *UPath, const char *Qfile, int isSOL)
{
     XrdOssCache_Group *cgp;
     long long bytesUsed;

// If usage directory or quota file was passed then we initialize space handling
// We need to create a space object to track usage across failures.
//
   if ((UPath || Qfile) && !XrdOssSpace::Init(UPath, Qfile, isSOL)) return 1;
   if (Qfile) Quotas = !isSOL;
   if (UPath) Usage  = 1;

// If we will be saving space information then we need to assign each group
// to a save set. If there is no space object then skip all of this.
//
   if (UPath && (cgp = XrdOssCache_Group::fsgroups))
      do {cgp->GRPid = XrdOssSpace::Assign(cgp->group, bytesUsed);
          cgp->Usage = bytesUsed;
         } while((cgp = cgp->next));
   return 0;
}

/******************************************************************************/

int XrdOssCache::Init(long long aMin, int ovhd, int aFuzz)
{
// Set values
//
   minAlloc = aMin;
   ovhAlloc = ovhd;
   fuzAlloc = static_cast<double>(aFuzz)/100.0;
   return 0;
}

/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/
  
void XrdOssCache::List(const char *lname, XrdSysError &Eroute)
{
     XrdOssCache_FS *fsp;
     const char *theCmd;
     char *pP, buff[4096];

     if ((fsp = fsfirst)) do
        {if (fsp->opts & XrdOssCache_FS::isXA)
            {pP = (char *)fsp->path + fsp->plen - 1;
             do {pP--;} while(*pP != '/');
             *pP = '\0';   theCmd = "space";
            } else {pP=0;  theCmd = "cache";}
         snprintf(buff, sizeof(buff), "%s%s %s %s", lname, theCmd,
                        fsp->group, fsp->path);
         if (pP) *pP = '/';
         Eroute.Say(buff);
         fsp = fsp->next;
        } while(fsp != fsfirst);
}
 
/******************************************************************************/
/*                                 P a r s e                                  */
/******************************************************************************/
  
char *XrdOssCache::Parse(const char *token, char *cbuff, int cblen)
{
   char *Path;

// Check for default
//
   if (!token || *token == ':')
      {strlcpy(cbuff, OSS_CGROUP_DEFAULT, cblen);
       return 0;
      }

// Get the correct cache group and partition path
//
   if (!(Path = (char *) index(token, ':'))) strlcpy(cbuff, token, cblen);
      else {int n = Path - token;
            if (n >= cblen) n = cblen-1;
            strncpy(cbuff, token, n); cbuff[n] = '\0';
            Path++;
           }

// All done
//
   return Path;
}

/******************************************************************************/
/*                                  S c a n                                   */
/******************************************************************************/

void *XrdOssCache::Scan(int cscanint)
{
   EPNAME("CacheScan")
   XrdOssCache_FSData *fsdp;
   XrdOssCache_Group  *fsgp;
   const struct timespec naptime = {cscanint, 0};
   long long frsz, llT; // llT is a dummy temporary
   int retc, dbgMsg, dbgNoMsg, dbgDoMsg;

// Try to prevent floodingthe log with scan messages
//
   if (cscanint > 60) dbgMsg = cscanint/60;
      else dbgMsg = 1;
   dbgNoMsg = dbgMsg;

// Loop scanning the cache
//
   while(1)
        {if (cscanint > 0) nanosleep(&naptime, 0);
         dbgDoMsg = !dbgNoMsg--;
         if (dbgDoMsg) dbgNoMsg = dbgMsg;

        // Get the cache context lock
        //
           Mutex.Lock();

        // Scan through all filesystems skip filesystem that have been
        // recently adjusted to avoid fs statstics latency problems.
        //
           fsSize =  0;
           fsTotFr=  0;
           fsFree =  0;
           fsdp = fsdata;
           while(fsdp)
                {retc = 0;
                 if ((fsdp->stat & XrdOssFSData_REFRESH)
                 || !(fsdp->stat & XrdOssFSData_ADJUSTED) || cscanint <= 0)
                     {frsz = XrdOssCache_FS::freeSpace(llT,fsdp->path);
                      if (frsz < 0) OssEroute.Emsg("CacheScan", errno ,
                                    "state file system ",(char *)fsdp->path);
                         else {fsdp->frsz = frsz;
                               fsdp->stat &= ~(XrdOssFSData_REFRESH |
                                               XrdOssFSData_ADJUSTED);
                               if (dbgDoMsg)
                                  {DEBUG("New free=" <<fsdp->frsz <<" path=" <<fsdp->path);}
                               }
                     } else fsdp->stat |= XrdOssFSData_REFRESH;
                 if (!retc)
                    {if (fsdp->frsz > fsFree)
                        {fsFree = fsdp->frsz; fsSize = fsdp->size;}
                     fsTotFr += fsdp->frsz;
                    }
                 fsdp = fsdp->next;
                }

        // Unlock the cache and if we have quotas check them out
        //
           Mutex.UnLock();
           if (cscanint <= 0) return (void *)0;
           if (Quotas) XrdOssSpace::Quotas();

        // Update usage information if we are keeping track of it
           if (Usage && XrdOssSpace::Readjust())
              {fsgp = XrdOssCache_Group::fsgroups;
               Mutex.Lock();
               while(fsgp)
                    {fsgp->Usage = XrdOssSpace::Usage(fsgp->GRPid);
                     fsgp = fsgp->next;
                    }
               Mutex.UnLock();
              }
        }

// Keep the compiler happy
//
   return (void *)0;
}
