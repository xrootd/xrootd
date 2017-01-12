/******************************************************************************/
/*                                                                            */
/*                       X r d P o s i x F i l e . c c                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include "XrdPosix/XrdPosixCallBack.hh"
#include "XrdPosix/XrdPosixFile.hh"
#include "XrdPosix/XrdPosixFileRH.hh"
#include "XrdPosix/XrdPosixPrepIO.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

namespace XrdPosixGlobals
{
extern XrdOucCache2 *theCache;
};

namespace
{
XrdPosixFile *InitDDL()
{
pthread_t tid;
XrdSysThread::Run(&tid, XrdPosixFile::DelayedDestroy, 0, 0, "PosixFileDestroy");
return (XrdPosixFile *)0;
}

std::string dsProperty("DataServer");
};

XrdSysSemaphore XrdPosixFile::ddSem(0);
XrdSysMutex     XrdPosixFile::ddMutex;
XrdPosixFile   *XrdPosixFile::ddList = InitDDL();

char          *XrdPosixFile::sfSFX    =  0;
int            XrdPosixFile::sfSLN    =  0;
bool           XrdPosixFile::ddPosted = false;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixFile::XrdPosixFile(const char *path, XrdPosixCallBack *cbP, int Opts)
             : XCio((XrdOucCacheIO2 *)this), PrepIO(0),
               mySize(0), myMtime(0), myInode(0), myMode(0),
               theCB(cbP),
               fPath(strdup(path)), fLoc(0),
               cOpt(0),
               isStream(Opts & isStrm ? 1 : 0)
{

// Check for structured file check
//
   if (sfSFX)
      {int n = strlen(path);
       if (n > sfSLN && !strcmp(sfSFX, path + n - sfSLN))
          cOpt = XrdOucCache::optFIS;
      }

// Set cache update option
//
   if (Opts & isUpdt) cOpt |= XrdOucCache::optRW;
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdPosixFile::~XrdPosixFile()
{
// Detach the cache if it is attached
//
   if (XCio != this) XCio->Detach();

// Close the remote connection
//
   if (clFile.IsOpen()) {XrdCl::XRootDStatus status = clFile.Close();};

// Get rid of defered open object
//
   if (PrepIO) delete PrepIO;

// Free the path and location information
//
   if (fPath) free(fPath);
   if (fLoc)  free(fLoc);
}

/******************************************************************************/
/*                        D e l a y e d D e s t r o y                         */
/******************************************************************************/

void* XrdPosixFile::DelayedDestroy(void* vpf)
{
// Static function.
// Called within a dedicated thread if XrdOucCacheIO is io-active or the
// file cannot be closed in a clean fashion for some reason.
//
   static const int ddInterval =  30;
   static const int maxTries   =  (3*60)/ddInterval;

   char eBuff[2048];
   XrdCl::XRootDStatus Status;
   const char *eTxt;
   XrdPosixFile *fCurr, *fNext;
   time_t tNow, wakeTime = 0;
   bool doWait = false;

// Wait for active I/O to complete
//
do{if (doWait)
      {sleep(ddInterval);
       doWait = false;
      } else {
       ddSem.Wait();
       tNow = time(0);
       if (tNow < wakeTime) continue;
       wakeTime = tNow + ddInterval;
      }

// Grab the delayed delete list
//
   ddMutex.Lock(); fNext=ddList; ddList=0; ddPosted=false; ddMutex.UnLock();

// Try to delete all the files on the list. If we exceeded the try limit,
// remove the file from the list and let it sit forever.
//
   while((fCurr = fNext))
        {fNext = fCurr->nextFile;
         if (!(fCurr->XCio->ioActive()) && !fCurr->Refs())
            {if (fCurr->Close(Status)) {delete fCurr; continue;}
                else eTxt = Status.ToString().c_str();
            } else   eTxt = "active I/O";

         if (fCurr->numTries > maxTries)
            {snprintf(eBuff, sizeof(eBuff),
                      "PosixFile: %s timeout closing %s; object lost!\n",
                      eTxt, fCurr->Path());
             std::cerr <<eBuff <<std::flush;
             fCurr->Close(Status);
            } else {
             fCurr->numTries++;
             doWait = true;
             ddMutex.Lock();
             fCurr->nextFile = ddList; ddList = fCurr; ddPosted = true;
             ddMutex.UnLock();
            }
        };
   } while(true);

   return 0;
}

/******************************************************************************/

void XrdPosixFile::DelayedDestroy(XrdPosixFile *fp)
{
   bool doPost;

// Place this file on the delayed delete list
//
   ddMutex.Lock();
   fp->nextFile = ddList;
   ddList       = fp;
   if (ddPosted) doPost = false;
      else {doPost   = true;
            ddPosted = true;
           }
   ddMutex.UnLock();
   fp->numTries = 0;

   if (doPost) ddSem.Post();
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

bool XrdPosixFile::Close(XrdCl::XRootDStatus &Status)
{
// If we don't need to close the file, then return success. Otherwise, do the
// actual close and return the status. We should have already been removed
// from the file table at this point and should be unlocked.
//
   if (clFile.IsOpen())
      {Status = clFile.Close();
       return Status.IsOK();
      }
   return true;
}
  
/******************************************************************************/
/*                              F i n a l i z e                               */
/******************************************************************************/

bool XrdPosixFile::Finalize(XrdCl::XRootDStatus *Status)
{
   XrdOucCacheIO2 *ioP;

// Indicate that we are at the start of the file
//
   currOffset = 0;

// Complete initialization. If the stat() fails, the caller will unwind the
// whole open process (ick). In the process get correct I/O vector.

        if (!Status)       ioP = (XrdOucCacheIO2 *)PrepIO;
   else if (Stat(*Status)) ioP = (XrdOucCacheIO2 *)this;
   else return false;

// Setup the cache if it is to be used
//
   if (XrdPosixGlobals::theCache)
      XCio = XrdPosixGlobals::theCache->Attach(ioP, cOpt);

   return true;
}
  
/******************************************************************************/
/*                                 F s t a t                                  */
/******************************************************************************/

int XrdPosixFile::Fstat(struct stat &buf)
{
   long long theSize;

// The size is treated differently here as it may come from a cache and may
// actually trigger a file open if the open was deferred.
//
   theSize = XCio->FSize();
   if (theSize < 0) return static_cast<int>(theSize);

// Return what little we can
//
   buf.st_size   = theSize;
   buf.st_atime  = buf.st_mtime = buf.st_ctime = myMtime;
   buf.st_blocks = buf.st_size/512+1;
   buf.st_ino    = myInode;
   buf.st_rdev   = myRdev;
   buf.st_mode   = myMode;
   return 0;
}
  
/******************************************************************************/
/*                        H a n d l e R e s p o n s e                         */
/******************************************************************************/
  
void XrdPosixFile::HandleResponse(XrdCl::XRootDStatus *status,
                                  XrdCl::AnyObject    *response)
{
   XrdCl::XRootDStatus Status;
   XrdPosixCallBack *xeqCB = theCB;
   int rc = fdNum;

// If no errors occured, complete the open
//
   if (!(status->IsOK()))          rc = XrdPosixMap::Result(*status);
      else if (!Finalize(&Status)) rc = XrdPosixMap::Result(Status);

// Issue callback with the correct result
//
   xeqCB->Complete(rc);

// Finish up
//
   delete status;
   delete response;
   if (rc) delete this;
}

/******************************************************************************/
/*                              L o c a t i o n                               */
/******************************************************************************/
  
const char *XrdPosixFile::Location()
{

// If the file is not open, then we have no location
//
   if (!clFile.IsOpen()) return 0;

// If we have no location info, get it
//
   if (!fLoc)
      {std::string currNode;
       if (clFile.GetProperty(dsProperty, currNode))
          fLoc = strdup(currNode.c_str());
      }

// Return location information
//
   return fLoc;
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

int XrdPosixFile::Read (char *Buff, long long Offs, int Len)
{
   XrdCl::XRootDStatus Status;
   uint32_t bytes;

// Issue read and return appropriately
//
   Status = clFile.Read((uint64_t)Offs, (uint32_t)Len, Buff, bytes);

   return (Status.IsOK() ? (int)bytes : XrdPosixMap::Result(Status));
}
  
/******************************************************************************/

void XrdPosixFile::Read (XrdOucCacheIOCB &iocb, char *buff, long long offs,
                         int rlen)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFileRH *rhp =  XrdPosixFileRH::Alloc(&iocb, this, offs, rlen,
                                                XrdPosixFileRH::isRead);

// Issue read
//
   Status = clFile.Read((uint64_t)offs, (uint32_t)rlen, buff, rhp);

// Check status
//
   if (!Status.IsOK()) rhp->Sched(-XrdPosixMap::Result(Status));
}

/******************************************************************************/
/*                                 R e a d V                                  */
/******************************************************************************/

int XrdPosixFile::ReadV (const XrdOucIOVec *readV, int n)
{
   XrdCl::XRootDStatus    Status;
   XrdCl::ChunkList       chunkVec;
   XrdCl::VectorReadInfo *vrInfo = 0;
   int nbytes = 0;

// Copy in the vector (would be nice if we didn't need to do this)
//
   chunkVec.reserve(n);
   for (int i = 0; i < n; i++)
       {nbytes += readV[i].size;
        chunkVec.push_back(XrdCl::ChunkInfo((uint64_t)readV[i].offset,
                                            (uint32_t)readV[i].size,
                                            (void   *)readV[i].data
                                           ));
       }

// Issue the readv. We immediately delete the vrInfo as w don't need it as a
// readv will succeed only if actually read the number of bytes requested.
//
   Status = clFile.VectorRead(chunkVec, (void *)0, vrInfo);
   delete vrInfo;

// Return appropriate result
//
   return (Status.IsOK() ? nbytes : XrdPosixMap::Result(Status));
}

/******************************************************************************/

void XrdPosixFile::ReadV(XrdOucCacheIOCB &iocb, const XrdOucIOVec *readV, int n)
{
   XrdCl::XRootDStatus    Status;
   XrdCl::ChunkList       chunkVec;
   int nbytes = 0;

// Copy in the vector (would be nice if we didn't need to do this)
//
   chunkVec.reserve(n);
   for (int i = 0; i < n; i++)
       {nbytes += readV[i].size;
        chunkVec.push_back(XrdCl::ChunkInfo((uint64_t)readV[i].offset,
                                            (uint32_t)readV[i].size,
                                            (void   *)readV[i].data
                                           ));
       }

// Issue the readv.
//
   XrdPosixFileRH *rhp =  XrdPosixFileRH::Alloc(&iocb, this, 0, nbytes,
                                                XrdPosixFileRH::isReadV);
   Status = clFile.VectorRead(chunkVec, (void *)0, rhp);

// Return appropriate result
//
   if (!Status.IsOK()) rhp->Sched(-XrdPosixMap::Result(Status));
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/

bool XrdPosixFile::Stat(XrdCl::XRootDStatus &Status, bool force)
{
   XrdCl::StatInfo *sInfo = 0;

// Get the stat information from the open file
//
   Status = clFile.Stat(force, sInfo);
   if (!Status.IsOK())
      {delete sInfo;
       return false;
      }

// Copy over the relevant fields
//
   myMode  = XrdPosixMap::Flags2Mode(&myRdev, sInfo->GetFlags());
   myMtime = static_cast<time_t>(sInfo->GetModTime());
   mySize  = static_cast<size_t>(sInfo->GetSize());
   myInode = static_cast<ino_t>(strtoll(sInfo->GetId().c_str(), 0, 10));

// Delete our status information and return final result
//
   delete sInfo;
   return true;
}

/******************************************************************************/
/*                                  S y n c                                   */
/******************************************************************************/

void XrdPosixFile::Sync(XrdOucCacheIOCB &iocb)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFileRH *rhp =  XrdPosixFileRH::Alloc(&iocb, this, 0, 0,
                                                XrdPosixFileRH::nonIO);

// Issue read
//
   Status = clFile.Sync(rhp);

// Check status
//
   if (!Status.IsOK()) rhp->Sched(-XrdPosixMap::Result(Status));
}
  
/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/

int XrdPosixFile::Write(char *Buff, long long Offs, int Len)
{
   XrdCl::XRootDStatus Status;

// Issue read and return appropriately
//
   Status = clFile.Write((uint64_t)Offs, (uint32_t)Len, Buff);

   return (Status.IsOK() ? Len : XrdPosixMap::Result(Status));
}
  
/******************************************************************************/

void XrdPosixFile::Write(XrdOucCacheIOCB &iocb, char *buff, long long offs,
                         int wlen)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFileRH *rhp =  XrdPosixFileRH::Alloc(&iocb, this, offs, wlen,
                                                XrdPosixFileRH::isWrite);

// Issue read
//
   Status = clFile.Write((uint64_t)offs, (uint32_t)wlen, buff, rhp);

// Check status
//
   if (!Status.IsOK()) rhp->Sched(-XrdPosixMap::Result(Status));
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
void XrdPosixFile::DoIt()
{
// Virtual function of XrdJob.
// Called from XrdPosixXrootd::Close if the file is still IO active.

   delete this;
}
