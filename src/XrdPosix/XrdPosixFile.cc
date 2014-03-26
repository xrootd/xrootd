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
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>

#include "XrdPosix/XrdPosixCallBack.hh"
#include "XrdPosix/XrdPosixFile.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdOucCache   *XrdPosixFile::CacheR   =  0;
XrdOucCache   *XrdPosixFile::CacheW   =  0;
char          *XrdPosixFile::sfSFX    =  0;
int            XrdPosixFile::sfSLN    =  0;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPosixFile::XrdPosixFile(const char *path, XrdPosixCallBack *cbP, int Opts)
             : XCio((XrdOucCacheIO *)this),
               mySize(0), myMtime(0), myInode(0), myMode(0),
               theCB(cbP),
               fPath(0),
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
   if (clFile.IsOpen()) clFile.Close();

// Free the path
//
   if (fPath) free(fPath);
}

/******************************************************************************/
/*                   D e l a y e d D e s t r o y                              */
/******************************************************************************/

void* XrdPosixFile::DelayedDestroy(void* vpf)
{
// Static function.
// Called within a dedicated thread if XrdOucCacheIO is io-active.

   XrdPosixFile* pf = (XrdPosixFile*)vpf;
   delete pf;

   return 0;
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

bool XrdPosixFile::Finalize(XrdCl::XRootDStatus &Status)
{
// Indicate that we are at the start of the file
//
   currOffset = 0;

// Complete initialization. If the stat() fails, the caller will unwind the
// whole open process (ick).

   if (!Stat(Status))
      return false;

// Setup the cache if it is to be used
//
   if (cOpt & XrdOucCache::optRW)
   {    if (CacheW) XCio = CacheW->Attach((XrdOucCacheIO *)this, cOpt);}
   else if (CacheR) XCio = CacheR->Attach((XrdOucCacheIO *)this, cOpt);


   return true;
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
   if (!(status->IsOK()))         rc = XrdPosixMap::Result(*status);
      else if (!Finalize(Status)) rc = XrdPosixMap::Result(Status);

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
/*                                  P a t h                                   */
/******************************************************************************/
  
const char *XrdPosixFile::Path()
{
   std::string fileUrl; clFile.GetProperty( "LastURL", fileUrl );
   if (!fPath) fPath = strdup(fileUrl.c_str());
   return fPath;
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
/*                                 R e a d V                                  */
/******************************************************************************/

int XrdPosixFile::ReadV (const XrdOucIOVec *readV, int n)
{
   XrdCl::XRootDStatus    Status;
   XrdCl::ChunkList       chunkVec;
   XrdCl::VectorReadInfo *vrInfo = 0;
   int i, nbytes = 0;

// Copy in the vector (would be nice if we didn't need to do this)
//
   chunkVec.reserve(n);
   for (i = 0; i < n; i++)
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
   myMode  = XrdPosixMap::Flags2Mode(sInfo->GetFlags());
   myMtime = static_cast<time_t>(sInfo->GetModTime());
   mySize  = static_cast<size_t>(sInfo->GetSize());
   myInode = static_cast<ino_t>(strtoll(sInfo->GetId().c_str(), 0, 10));

// Delete our status information and return final result
//
   delete sInfo;
   return true;
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
