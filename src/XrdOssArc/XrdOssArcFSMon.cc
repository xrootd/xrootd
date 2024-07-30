/******************************************************************************/
/*                                                                            */
/*                     X r d O s s A r c F S M o n . c c                      */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/*        P l a t f o r m   D e p e n d e n t   D e f i n i t i o n s         */
/******************************************************************************/

// This should really be part of XrdSysPlatform.hh (see XrdOssCache.hh).
//
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
#include <sys/vfs.h>
#define FS_Stat(a,b) statfs(a,b)
#define STATFS_t struct statfs
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
/*                              I n c l u d e s                               */
/******************************************************************************/

#include <stdio.h>

#include "Xrd/XrdScheduler.hh"
  
#include "XrdOssArc/XrdOssArcBackup.hh"
#include "XrdOssArc/XrdOssArcFSMon.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdScheduler*   schedP;

extern XrdSysError     Elog;

extern XrdSysTrace     ArcTrace;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
/*           L o c a l   O b j e c t s   &   D e f i n i t i o n s            */
/******************************************************************************/

#define HSZ(x,y) XrdOucUtils::HSize(x, y, (int)sizeof(y))
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/

void  XrdOssArcFSMon::DoIt()
{
   TraceInfo("FSMon", 0);
   XrdSysMutexHelper rmHelp(rmMutex);
   size_t tmpSize, tmpFree;

// Update file system  statistics
//
   if (!(tmpSize = getFSpace(tmpFree, fs_Path)))
      Elog.Emsg("FSMon", errno, "filesystem info for", fs_Path);
      else {fs_Size = tmpSize;
            fs_Free = tmpFree;
            fs_MaxUsed = fs_Size - fs_MinFree;
            fs_inUse   = fs_Size - fs_Free;
           }

// Perform some debugging
//
   DEBUG("FS info: Size="<<fs_Size<<" Free="<<fs_Free<<" Used="<<fs_inUse<<
         " Commit="<<fs_inBkp<<" Avail="<<
         (fs_Free <= fs_inBkp ? 0 : fs_Free - fs_inBkp));

// reschedule ourselves
//
   schedP->Schedule(this, time(0)+fs_Updt);
}
  
/******************************************************************************/
/* Private:                    g e t F S p a c e                              */
/******************************************************************************/
  
size_t XrdOssArcFSMon::getFSpace(size_t &Free, const char *path)
{
   STATFS_t fsbuff;

// Get space information for the requested filesystem
//
   if (FS_Stat(path, &fsbuff)) return 0;
   Free = static_cast<size_t>(fsbuff.f_bavail)
        * static_cast<size_t>(fsbuff.FS_BLKSZ);
   return static_cast<size_t>(fsbuff.f_blocks)
        * static_cast<size_t>(fsbuff.FS_BLKSZ);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

bool XrdOssArcFSMon::Init(const char* path, long long fVal, int fsupdt)
{
   char buff[1024], HSb1[16], HSb2[16], HSb3[16];

// Save the update frequescy
//
   fs_Updt = fsupdt;

// The first step is to get the relevant filesystem statistics we need
//
   if (!(fs_Size = getFSpace(fs_Free, path)))
      {Elog.Emsg("FSMon", errno, "filesystem info for", path);
       return false;
      }

// Calculate the minimum free space allowed (if fval < 0 -> percentage)
//
   if (fVal < 0) fs_MinFree = fs_Size*(static_cast<size_t>(-fVal))/100;
      else {fs_MinFree = static_cast<size_t>(fVal);
            if (fs_MinFree >= fs_Size)
               {snprintf(buff, sizeof(buff), "Minimum free space allowed (%s) "
                              ">= size of filesystem (%s) at",
                               HSZ(fs_MinFree, HSb1), HSZ(fs_Size, HSb2));
               Elog.Emsg("FSMon", buff, path);
               return false;
              }
           }

// Calculate the maximum amount of usage and set the filesystem path
//
   fs_MaxUsed = fs_Size - fs_MinFree;
   fs_inUse   = fs_Size - fs_Free;
   fs_Path = path;

// Check if we don't have enough free space at startup time
//
   if (fs_Free < fs_MinFree)
      {snprintf(buff, sizeof(buff), "Filesystme free space (%s) < minimum "
                      "allowed (%s) at ",
                       HSZ(fs_Free, HSb1), HSZ(fs_MinFree, HSb2));     
       Elog.Say("Config warning: ", buff, path);
      } else {
       snprintf(buff, sizeof(buff), "Filesystme free space: %s; "
                      "minimum allowed: %s; remaining: %s at ",
                      HSZ(fs_Free, HSb1), HSZ(fs_MinFree,HSb2),
                      HSZ(fs_Free-fs_MinFree, HSb3));
       Elog.Say("Config outcome: ", buff, path);
      }

// Start automatic filesystem updates
//
   schedP->Schedule(this, time(0)+fs_Updt);

// All done
//
   return true;     
}

/******************************************************************************/
/*                                P e r m i t                                 */
/******************************************************************************/

bool XrdOssArcFSMon::Permit(XrdOssArcBackupTask* btP)
{
// Check if we can permit this request or the caller will need to wait
//
   rmMutex.Lock();
   if ((fs_inUse + fs_inBkp + btP->numBytes) <= fs_MaxUsed)
      {fs_inBkp += btP->numBytes;
       btP->relSpace = true;
       rmMutex.UnLock();
       return true;
      }

// There is not enough space to permit the request. Place it on the re-drive
// queue and tell the call to wait.
//
   btWaitQ.push_back(btP);
   rmMutex.UnLock();
   return false;
}

/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
void XrdOssArcFSMon::Release(size_t bytes)
{
   XrdSysMutexHelper mHelp(rmMutex);
   size_t tmpSize, tmpFree;
   int n;

// Release the number of bytes previously reserved
//
   if (bytes >= fs_inBkp) fs_inBkp = 0;
      else fs_inBkp -= bytes;

// Update file system  statistics
//
   if (!(tmpSize = getFSpace(tmpFree, fs_Path)))
      Elog.Emsg("FSMon", errno, "filesystem info for", fs_Path);
      else {fs_Size = tmpSize;
            fs_Free = tmpFree;
            fs_MaxUsed = fs_Size - fs_MinFree;
            fs_inUse   = fs_Size - fs_Free;
           }

// If a backup is waiting for space, see if it can proceed
//
   size_t nTot = 0;
   while(!btWaitQ.empty())
      {XrdOssArcBackupTask* btP = btWaitQ.front();
       if ((fs_inUse + fs_inBkp + btP->numBytes + nTot) <= fs_MaxUsed)
          {nTot += btP->numBytes;
           btP->btSem.Post();
           btWaitQ.pop_front();
          } else break;
      }

// Issue message if there are still queued backups
//
   if ((n = btWaitQ.size()))
      {char buff[1024], HSb1[16], HSb2[16];
       size_t free = (fs_Free <= fs_inBkp ? 0 : fs_Free - fs_inBkp); 
       snprintf(buff, sizeof(buff), "Insufficient free space (%s < %s); "
                      "%d backup(s) still pending!",
                       HSZ(free, HSb1), HSZ(fs_MinFree, HSb2), n);
       Elog.Emsg("FSMon", buff);
      }
}
