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

#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdPosix/XrdPosixCallBack.hh"
#include "XrdPosix/XrdPosixConfig.hh"
#include "XrdPosix/XrdPosixFile.hh"
#include "XrdPosix/XrdPosixFileRH.hh"
#include "XrdPosix/XrdPosixPrepIO.hh"
#include "XrdPosix/XrdPosixStats.hh"
#include "XrdPosix/XrdPosixTrace.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

namespace XrdPosixGlobals
{
extern XrdOucCache     *theCache;
extern XrdOucName2Name *theN2N;
extern XrdSysError     *eDest;
extern XrdPosixStats    Stats;
extern int              ddInterval;
extern int              ddMaxTries;
extern bool             autoPGRD;
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
XrdPosixFile   *XrdPosixFile::ddLost = 0;

char          *XrdPosixFile::sfSFX    =  0;
short          XrdPosixFile::sfSLN    =  0;
bool           XrdPosixFile::ddPosted = false;
int            XrdPosixFile::ddNum    =  0;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
namespace
{
class pgioCB : public XrdOucCacheIOCB
{
public:

void Done(int result)
         {rc = result; pgSem.Post();}

int  Wait4PGIO()      {pgSem.Wait(); return rc;}

     pgioCB(const char *who) : pgSem(0, who), rc(0) {}
    ~pgioCB() {}

private:

XrdSysSemaphore pgSem;
int             rc;
};
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixFile::XrdPosixFile(bool &aOK, const char *path, XrdPosixCallBack *cbP,
                           int Opts)
             : XCio((XrdOucCacheIO *)this), PrepIO(0),
               mySize(0), myAtime(0), myCtime(0), myMtime(0), myRdev(0),
               myInode(0), myMode(0), theCB(cbP), fLoc(0), cOpt(0),
               isStream(Opts & isStrm ? 1 : 0)
{
// Handle path generation. This is trickt as we may have two namespaces. One
// for the origin and one for the cache.
//
   fOpen = strdup(path); aOK = true;
   if (!XrdPosixGlobals::theN2N || !XrdPosixGlobals::theCache) fPath = fOpen;
      else if (!XrdPosixXrootPath::P2L("file",path,fPath)) aOK = false;
              else if (!fPath) fPath = fOpen;

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
// Close the remote connection
//
   if (clFile.IsOpen())
      {XrdPosixGlobals::Stats.Count(XrdPosixGlobals::Stats.X.Closes);
       XrdCl::XRootDStatus status = clFile.Close();
       if (!status.IsOK())
          XrdPosixGlobals::Stats.Count(XrdPosixGlobals::Stats.X.CloseErrs);
      }

// Get rid of deferred open object
//
   if (PrepIO) delete PrepIO;

// Free the path and location information
//
   if (fPath) free(fPath);
   if (fOpen != fPath) free(fOpen);
   if (fLoc)  free(fLoc);
}

/******************************************************************************/
/*                        D e l a y e d D e s t r o y                         */
/******************************************************************************/

void* XrdPosixFile::DelayedDestroy(void* vpf)
{
// Static function.
// Called within a dedicated thread if there is a reference outstanding to the
// file or the file cannot be closed in a clean fashion for some reason.
//
   EPNAME("DDestroy");

   XrdSysError *Say = XrdPosixGlobals::eDest;
   XrdCl::XRootDStatus Status;
   std::string statusMsg;
   const char *eTxt;
   XrdPosixFile *fCurr, *fNext;
   char buff[512], buff2[256];
   static int ddNumLost = 0;
   int ddCount, refNum;
   bool doWait = false;

// Wait for active I/O to complete
//
do{if (doWait)
      {XrdSysTimer::Snooze(XrdPosixGlobals::ddInterval);
       doWait = false;
      } else {
       ddSem.Wait();
       doWait = true;
       continue;
      }

// Grab the delayed delete list
//
   ddMutex.Lock();
   fNext=ddList; ddList=0; ddPosted=false; ddCount = ddNum; ddNum = 0;
   ddMutex.UnLock();

// Do some debugging
//
   DEBUG("DLY destroy of "<<ddCount<<" objects; "<<ddNumLost <<" already lost.");

// Try to delete all the files on the list. If we exceeded the try limit,
// remove the file from the list and let it sit forever.
//
   int nowLost = ddNumLost;
   while((fCurr = fNext))
        {fNext = fCurr->nextFile;
         if (!(refNum = fCurr->Refs()))
            {if (fCurr->Close(Status) || !fCurr->clFile.IsOpen())
                {delete fCurr; ddCount--; continue;}
                else {statusMsg = Status.ToString();
                      eTxt = statusMsg.c_str();
                     }
            } else eTxt = 0;

         if (fCurr->numTries > XrdPosixGlobals::ddMaxTries)
            {ddNumLost++; ddCount--;
             if (!eTxt)
                {snprintf(buff2, sizeof(buff2), "in use %d", refNum);
                 eTxt = buff2;
                }
             if (Say)
                {snprintf(buff, sizeof(buff), "%s timeout closing", eTxt);
                 Say->Emsg("DDestroy", buff, fCurr->Origin());
                } else {
                 DMSG("DDestroy", eTxt <<" timeout closing " <<fCurr->Origin()
                        <<' ' <<ddNumLost <<" objects lost");
                }
             fCurr->nextFile = ddLost;
             ddLost = fCurr;
            } else {
             fCurr->numTries++;
             doWait = true;
             ddMutex.Lock();
             fCurr->nextFile = ddList; ddList = fCurr;
             ddNum++; ddPosted = true;
             ddMutex.UnLock();
            }
        }
        if (Say && ddNumLost - nowLost >= 3)
           {snprintf(buff, sizeof(buff), "%d objects deferred and %d lost.",
                                         ddCount,  ddNumLost);
            Say->Emsg("DDestroy", buff);
           } else {
            DEBUG("DLY destroy end; "<<ddCount<<" objects deferred and "
                             <<ddNumLost <<" lost.");
           }
        if (XrdPosixGlobals::theCache && ddNumLost != nowLost)
           XrdPosixGlobals::theCache->Statistics.Set(
          (XrdPosixGlobals::theCache->Statistics.X.ClosedLost), ddNumLost);
   } while(true);

   return 0;
}

/******************************************************************************/

void XrdPosixFile::DelayedDestroy(XrdPosixFile *fp)
{
   EPNAME("DDestroyFP");
   int  ddCount;
   bool doPost;

// Count number of times this has happened (we should have a cache)
//
   if (XrdPosixGlobals::theCache)
       XrdPosixGlobals::theCache->Statistics.Count(
         (XrdPosixGlobals::theCache->Statistics.X.ClosDefers));

// Place this file on the delayed delete list
//
   ddMutex.Lock();
   fp->nextFile = ddList;
   ddList       = fp;
   ddNum++; ddCount = ddNum;
   if (ddPosted) doPost = false;
      else {doPost   = true;
            ddPosted = true;
           }
   fp->numTries = 0;
   ddMutex.UnLock();

   DEBUG("DLY destroy "<<(doPost ? "post " : "has ")<<ddCount
                       <<" objects; added "<<fp->Origin());

   if (doPost) ddSem.Post();
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

bool XrdPosixFile::Close(XrdCl::XRootDStatus &Status)
{
// If this is a deferred open, disable any future calls as we are ready to
// shutdown this beast!
//
   if (PrepIO) PrepIO->Disable();

// If we don't need to close the file, then return success. Otherwise, do the
// actual close and return the status. We should have already been removed
// from the file table at this point and should be unlocked.
//
   if (clFile.IsOpen())
      {XrdPosixGlobals::Stats.Count(XrdPosixGlobals::Stats.X.Closes);
       Status = clFile.Close();
       if (Status.IsOK()) return true;
       XrdPosixGlobals::Stats.Count(XrdPosixGlobals::Stats.X.CloseErrs);
       return false;
      }
   return true;
}
  
/******************************************************************************/
/*                              F i n a l i z e                               */
/******************************************************************************/

bool XrdPosixFile::Finalize(XrdCl::XRootDStatus *Status)
{
   XrdOucCacheIO *ioP;

// Indicate that we are at the start of the file
//
   currOffset = 0;

// Complete initialization. If the stat() fails, the caller will unwind the
// whole open process (ick). In the process get correct I/O vector.

        if (!Status)       ioP = (XrdOucCacheIO *)PrepIO;
   else if (Stat(*Status)) ioP = (XrdOucCacheIO *)this;
   else return false;

// Setup the cache if it is to be used
//
   if (XrdPosixGlobals::theCache)
      {XCio = XrdPosixGlobals::theCache->Attach(ioP, cOpt);
       if (ioP == (XrdOucCacheIO *)PrepIO)
       XrdPosixGlobals::theCache->Statistics.Add(
         (XrdPosixGlobals::theCache->Statistics.X.OpenDefers), 1LL);
      }

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
   XrdPosixConfig::initStat(&buf);
   buf.st_size   = theSize;
   buf.st_atime  = myAtime;
   buf.st_ctime  = myCtime;
   buf.st_mtime  = myMtime;
   buf.st_blocks = buf.st_size/512 + buf.st_size%512;
   buf.st_ino    = myInode;
   buf.st_rdev   = myRdev;
   buf.st_mode   = myMode;
   return 0;
}
  
/******************************************************************************/
/*                        H a n d l e R e s p o n s e                         */
/******************************************************************************/

// Note: This response handler is only used for async open requests!
  
void XrdPosixFile::HandleResponse(XrdCl::XRootDStatus *status,
                                  XrdCl::AnyObject    *response)
{
   XrdCl::XRootDStatus Status;
   XrdPosixCallBack *xeqCB = theCB;
   int rc = fdNum;

// If no errors occurred, complete the open
//
   if (!(status->IsOK()))          rc = XrdPosixMap::Result(*status);
      else if (!Finalize(&Status)) rc = XrdPosixMap::Result(Status);

// Issue XrdPosixCallBack callback with the correct result. Note: errors are
// indicated with result set to -1 and errno set to the error number.
//
   xeqCB->Complete(rc);

// Finish up
//
   delete status;
   delete response;
   if (rc < 0) delete this;
}

/******************************************************************************/
/*                              L o c a t i o n                               */
/******************************************************************************/
  
const char *XrdPosixFile::Location(bool refresh)
{

// If the file is not open, then we have no location
//
   if (!clFile.IsOpen()) return "";

// If we have no location info, get it
//
   if (!fLoc || refresh)
      {std::string currNode;
       if (clFile.GetProperty(dsProperty, currNode))
          {if (!fLoc || strcmp(fLoc, currNode.c_str()))
              {if (fLoc) free(fLoc);
               fLoc = strdup(currNode.c_str());
              }
          } else return "";
      }

// Return location information
//
   return fLoc;
}

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/

int XrdPosixFile::pgRead(char                  *buff,
                         long long              offs,
                         int                    rlen,
                         std::vector<uint32_t> &csvec,
                         uint64_t               opts,
                         int                   *csfix)
{
// Do a sync call using the async interface
//
   pgioCB pgrCB("Posix pgRead CB");
   pgRead(pgrCB, buff, offs, rlen, csvec, opts, csfix);
   return pgrCB.Wait4PGIO();
}
  
/******************************************************************************/

void XrdPosixFile::pgRead(XrdOucCacheIOCB       &iocb,
                          char                  *buff,
                          long long              offs,
                          int                    rlen,
                          std::vector<uint32_t> &csvec,
                          uint64_t               opts,
                          int                   *csfix)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFileRH *rhP;

// Allocate callback object. Note the response handler may do additional post
// processing.
//
   rhP = XrdPosixFileRH::Alloc(&iocb, this, offs, rlen, XrdPosixFileRH::isReadP);

// Set the destination checksum vector
//
   if (csfix) *csfix = 0;
   rhP->setCSVec(&csvec, csfix, (opts & XrdOucCacheIO::forceCS) != 0);

// Issue read
//
   Ref();
   Status = clFile.PgRead((uint64_t)offs,(uint32_t)rlen,buff,rhP);

// Check status
//
   if (!Status.IsOK())
      {rhP->Sched(XrdPosixMap::Result(Status, false));
       unRef();
      }
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/

int XrdPosixFile::pgWrite(char                  *buff,
                          long long              offs,
                          int                    wlen,
                          std::vector<uint32_t> &csvec,
                          uint64_t               opts,
                          int                   *csfix)
{
   XrdCl::XRootDStatus Status;

// Preset checksum error count
//
   if (csfix) *csfix = 0;

// Issue write and return appropriately
//
   Ref();
   Status = clFile.PgWrite((uint64_t)offs, (uint32_t)wlen, buff, csvec);
   unRef();

   return (Status.IsOK() ? wlen : XrdPosixMap::Result(Status));
}
  
/******************************************************************************/

void XrdPosixFile::pgWrite(XrdOucCacheIOCB       &iocb,
                           char                  *buff,
                           long long              offs,
                           int                    wlen,
                           std::vector<uint32_t> &csvec,
                           uint64_t               opts,
                           int                   *csfix)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFileRH *rhP;

// Allocate callback object. Note that a pgWrite is essentially a normal write
// as far as the response handler is concerned.
//
   rhP = XrdPosixFileRH::Alloc(&iocb,this,offs,wlen,XrdPosixFileRH::isWrite);

// Set checksum info
//
   if (csfix)
      {*csfix = 0;
       rhP->setCSVec(0, csfix);
      }

// Issue write
//
   Ref();
   Status = clFile.PgWrite((uint64_t)offs, (uint32_t)wlen, buff, csvec, rhP);

// Check status
//
   if (!Status.IsOK())
      {rhP->Sched(XrdPosixMap::Result(Status));
       unRef();
      }
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

int XrdPosixFile::Read (char *Buff, long long Offs, int Len)
{
   XrdCl::XRootDStatus Status;
   uint32_t bytes;

// Handle automatic pgread
//
   if (XrdPosixGlobals::autoPGRD)
      {pgioCB pgrCB("Posix pgRead CB");
       Read(pgrCB, Buff, Offs, Len);
       return pgrCB.Wait4PGIO();
      }

// Issue read and return appropriately.
//
   Ref();
   Status = clFile.Read((uint64_t)Offs, (uint32_t)Len, Buff, bytes);
   unRef();

   return (Status.IsOK() ? (int)bytes : XrdPosixMap::Result(Status, false));
}
  
/******************************************************************************/

void XrdPosixFile::Read (XrdOucCacheIOCB &iocb, char *buff, long long offs,
                         int rlen)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFileRH *rhP;
   XrdPosixFileRH::ioType rhT;
   bool doPgRd = XrdPosixGlobals::autoPGRD;

// Allocate correct callback object
//
   rhT = (doPgRd ? XrdPosixFileRH::isReadP : XrdPosixFileRH::isRead);
   rhP = XrdPosixFileRH::Alloc(&iocb, this, offs, rlen, rhT);

// Issue read
//
   Ref();
   if (doPgRd) Status = clFile.PgRead((uint64_t)offs,(uint32_t)rlen,buff,rhP);
      else     Status = clFile.Read  ((uint64_t)offs,(uint32_t)rlen,buff,rhP);

// Check status
//
   if (!Status.IsOK())
      {rhP->Sched(XrdPosixMap::Result(Status, false));
       unRef();
      }
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
   Ref();
   Status = clFile.VectorRead(chunkVec, (void *)0, vrInfo);
   unRef();
   delete vrInfo;

// Return appropriate result
//
   return (Status.IsOK() ? nbytes : XrdPosixMap::Result(Status, false));
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
   Ref();
   Status = clFile.VectorRead(chunkVec, (void *)0, rhp);

// Return appropriate result
//
   if (!Status.IsOK())
      {rhp->Sched(XrdPosixMap::Result(Status, false));
       unRef();
      }
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/

bool XrdPosixFile::Stat(XrdCl::XRootDStatus &Status, bool force)
{
   XrdCl::StatInfo *sInfo = 0;

// Get the stat information from the open file
//
   Ref();
   Status = clFile.Stat(force, sInfo);
   if (!Status.IsOK())
      {unRef();
       delete sInfo;
       return false;
      }

// Copy over the relevant fields, the stat structure must have been
// properly pre-initialized.
//
   myMode  = XrdPosixMap::Flags2Mode(&myRdev, sInfo->GetFlags());
   myMtime = static_cast<time_t>(sInfo->GetModTime());
   mySize  = static_cast<size_t>(sInfo->GetSize());
   myInode = static_cast<ino_t>(strtoll(sInfo->GetId().c_str(), 0, 10));

// If this is an extended stat then we can get some more info
//
   if (sInfo->ExtendedFormat())
      {myCtime = static_cast<time_t>(sInfo->GetChangeTime());
       myAtime = static_cast<time_t>(sInfo->GetAccessTime());
      } else {
       myCtime = myMtime;
       myAtime = time(0);
      }

// Delete our status information and return final result
//
   unRef();
   delete sInfo;
   return true;
}

/******************************************************************************/
/*                                  S y n c                                   */
/******************************************************************************/

int XrdPosixFile::Sync()
{
   XrdCl::XRootDStatus Status;

// Issue the Sync
//
   Ref();
   Status = clFile.Sync();
   unRef();

// Return result
//
   return XrdPosixMap::Result(Status, false);
}

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
   if (!Status.IsOK()) rhp->Sched(XrdPosixMap::Result(Status, false));
}
  
/******************************************************************************/
/*                                 T r u n c                                  */
/******************************************************************************/

int XrdPosixFile::Trunc(long long Offset)
{
   XrdCl::XRootDStatus Status;

// Issue truncate request
//
   Ref();
   Status = clFile.Truncate((uint64_t)Offset);
   unRef();

// Return results
//
   return XrdPosixMap::Result(Status);
}
  
/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/

int XrdPosixFile::Write(char *Buff, long long Offs, int Len)
{
   XrdCl::XRootDStatus Status;

// Issue write and return appropriately
//
   Ref();
   Status = clFile.Write((uint64_t)Offs, (uint32_t)Len, Buff);
   unRef();

   return (Status.IsOK() ? Len : XrdPosixMap::Result(Status));
}
  
/******************************************************************************/

void XrdPosixFile::Write(XrdOucCacheIOCB &iocb, char *buff, long long offs,
                         int wlen)
{
   XrdCl::XRootDStatus Status;
   XrdPosixFileRH *rhp =  XrdPosixFileRH::Alloc(&iocb, this, offs, wlen,
                                                XrdPosixFileRH::isWrite);

// Issue write
//
   Ref();
   Status = clFile.Write((uint64_t)offs, (uint32_t)wlen, buff, rhp);

// Check status
//
   if (!Status.IsOK())
      {rhp->Sched(XrdPosixMap::Result(Status));
       unRef();
      }
}
