/******************************************************************************/
/*                                                                            */
/*                       X r d P o x F i l e R H . c c                        */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <utility>
#include <vector>

#include "Xrd/XrdScheduler.hh"

#include "XrdOuc/XrdOucCache.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"

#include "XrdPosix/XrdPosixFileRH.hh"
#include "XrdPosix/XrdPosixFile.hh"
#include "XrdPosix/XrdPosixMap.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

namespace XrdPosixGlobals
{
extern           XrdScheduler  *schedP;
};

XrdSysMutex      XrdPosixFileRH::myMutex;
XrdPosixFileRH  *XrdPosixFileRH::freeRH   =   0;
int              XrdPosixFileRH::numFree  =   0;
int              XrdPosixFileRH::maxFree  = 100;

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/
  
namespace
{
void *callDoIt(void *pp)
{
     XrdPosixFileRH *rhp = (XrdPosixFileRH *)pp;
     rhp->DoIt();
     return (void *)0;
}
};
  
/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdPosixFileRH *XrdPosixFileRH::Alloc(XrdOucCacheIOCB *cbp,
                                      XrdPosixFile    *fp,
                                      long long        offs,
                                      int              xResult,
                                      ioType           typeIO)
{
   XrdPosixFileRH *newCB;

// Try to allocate an prexisting object otherwise get a new one
//
   myMutex.Lock();
   if ((newCB = freeRH)) {freeRH = newCB->next; numFree--;}
      else newCB = new XrdPosixFileRH;
   myMutex.UnLock();

// Initialize the callback and return it
//
   newCB->theCB   = cbp;
   newCB->theFile = fp;
   newCB->csVec   = 0;
   newCB->csfix   = 0;
   newCB->offset  = offs;
   newCB->result  = xResult;
   newCB->typeIO  = typeIO;
   newCB->csFrc   = false;
   return newCB;
}
  
/******************************************************************************/
/*                        H a n d l e R e s p o n s e                         */
/******************************************************************************/
  
void XrdPosixFileRH::HandleResponse(XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject    *response)
{

// Determine ending status. Note: error indicated as result set to -errno.
//
        if (!(status->IsOK())) result = XrdPosixMap::Result(*status, false);
   else if (typeIO == nonIO) result = 0;
   else if (typeIO == isRead)
           {XrdCl::ChunkInfo *cInfo = 0;
            union {uint32_t ubRead; int ibRead;};
            response->Get(cInfo);
            ubRead = (cInfo ? cInfo->length : 0);
            result = ibRead;
           }
   else if (typeIO == isReadP)
           {XrdCl::PageInfo *pInfo = 0;
            union {uint32_t ubRead; int ibRead;};
            response->Get(pInfo);
            if (pInfo)
               {ubRead = pInfo->GetLength();
                result = ibRead;
                if (csVec) 
                   {if (!csFrc || pInfo->GetCksums().size() != 0 || result <= 0)
                       *csVec = std::move(pInfo->GetCksums() );
                       else {uint64_t offs = pInfo->GetOffset();
                             void *buff = pInfo->GetBuffer();
                             XrdOucPgrwUtils::csCalc((const char *)buff,
                                                     (ssize_t)offs, ubRead,
                                                     *csVec);
                            }
                    csVec = 0;
                   }
                if (csfix) *csfix = pInfo->GetNbRepair();
               } else {
                result = 0;
                if (csVec) {csVec->clear(); csVec = 0;}
               }
           }
   else if (typeIO == isWrite) theFile->UpdtSize(offset+result);

// Get rid of things we don't need
//
   delete status;
   delete response;

// Now schedule our XrdOucCacheIOCB callback
//
   theFile->unRef();
   if (XrdPosixGlobals::schedP) XrdPosixGlobals::schedP->Schedule(this);
      else {pthread_t tid;
            XrdSysThread::Run(&tid, callDoIt, this, 0, "PosixFileRH");
           }
}
  
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdPosixFileRH::Recycle()
{
// Perform recycling
//
   myMutex.Lock();
   if (numFree >= maxFree) delete this;
      else {next   = freeRH;
            freeRH = this;
            numFree++;
           }
   myMutex.UnLock();
}

/******************************************************************************/
/*                                 S c h e d                                  */
/******************************************************************************/
  
void XrdPosixFileRH::Sched(int rval)
{
// Set result
//
   result = rval;

// Now schedule this callback
//
   if (XrdPosixGlobals::schedP) XrdPosixGlobals::schedP->Schedule(this);
      else {pthread_t tid;
            XrdSysThread::Run(&tid, callDoIt, this, 0, "PosixFileRH");
           }
}
