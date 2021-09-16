/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d P g w C t l . c c                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>

#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdPgwCtl.hh"
#include "XrdXrootd/XrdXrootdPgwFob.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
const char *XrdXrootdPgwCtl::TraceID = "pgwCtl";

namespace
{
static const int pgPageSize = XrdProto::kXR_pgPageSZ;
static const int pgPageMask = XrdProto::kXR_pgPageSZ-1;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdPgwCtl::XrdXrootdPgwCtl(int pid)
                : XrdXrootdPgwBadCS(pid), dataBuff(0), dataBLen(0), fixSRD(0)
{

// Clear the response area
//
   memset(&resp, 0, sizeof(resp));

// Setup response fields that stay constant for the life of the object
//
   resp.bdy.requestid = kXR_pgwrite - kXR_1stRequest;
   resp.bdy.resptype  = XrdProto::kXR_FinalResult; // No partials

// Setup the iovec assuming full usage
//
   kXR_unt32 *csP = csVec;
   for (int i = 0; i < maxIOVN; i += 2)
       {ioVec[i  ].iov_base = csP++;
        ioVec[i  ].iov_len  = sizeof(kXR_unt32);
        ioVec[i+1].iov_len  = pgPageSize;
       };
}

/******************************************************************************/
/*                               A d v a n c e                                */
/******************************************************************************/
  
bool XrdXrootdPgwCtl::Advance()
{
// Check if we have anything to advance here
//
   if (iovRem <= 0)
      {iovNum = 0;
       iovLen = 0;
       return false;
      }

// Readjust values for first data iov element as the previous one may not have
// bin for a full page (unaligned read). We just do it categorically.
//
   ioVec[1].iov_base = dataBuff;
   ioVec[1].iov_len  = pgPageSize;

// Compute number of iovec element we will use for the next read.
//
   if (iovRem > iovNum) iovRem -= iovNum;
      else {iovNum = iovRem;
            iovRem = 0;
            if (endLen)
               {ioVec[iovNum-1].iov_len = endLen;
                fixSRD = iovNum-1;
               }
           }

// Compute bytes read by this frame
//
   int n  = iovNum>>1;
   iovLen = ioVec[iovNum-1].iov_len + (n*crcSZ);
   if (n > 1) iovLen += (n-1)*pgPageSize;

// Indicate there is more to do
//
   return true;
}
  
/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
const char *XrdXrootdPgwCtl::Setup(XrdBuffer *buffP, kXR_int64 fOffs, int totlen)
{
   XrdOucPgrwUtils::Layout layout;
   int csNum, iovMax;

// Reset short length in the iovec from the last use.
//
   if (fixSRD)
      {ioVec[fixSRD].iov_len = pgPageSize;
       fixSRD = 0;
      }

// Compute the layout parameters for the complete read (done once)
//
   if (!(csNum = XrdOucPgrwUtils::recvLayout(layout, fOffs, totlen)))
      return layout.eWhy;

// Compute the maximum number of iov entries for the real buffer size
//
   if (buffP->bsize >= maxBSize) iovMax = (maxBSize/XrdProto::kXR_pgPageSZ)*2;
      else iovMax = (buffP->bsize/XrdProto::kXR_pgPageSZ)*2;

// Verify the logic here, under no circumstance should iovMax be zero
//
   if (!iovMax) return "PgwCtl logic error detected; buffer is too small";

// If the buffer has changed, then we must update buffer addresses in the iovec
// Note that buffer sizes are always a power of 1K (i.e. 1, 2, 4, 8, etc).
// However, the caller is on the hook to make the buffer no less than 4K.
//
   if (buffP->buff != dataBuff || buffP->bsize != dataBLen)
      {char *dP;
       dP = dataBuff = buffP->buff; dataBLen = buffP->bsize;
       for (int i = 1; i < iovMax; i +=2)
           {ioVec[i].iov_base = dP;
            dP += XrdProto::kXR_pgPageSZ;
           }
      }

// Setup control information and preset the initial read.
//
   ioVec[1].iov_base = buffP->buff + layout.bOffset;
   ioVec[1].iov_len  = layout.fLen;

// Now setup for subsequent reads which we may not need.
//
   iovRem = csNum<<1;
   if (iovRem > iovMax)
      {iovNum  = iovMax;
       iovLen  = layout.fLen + ((iovMax/2-1)*pgPageSize) + (iovMax/2*crcSZ);
       endLen  = layout.lLen;
      } else {
       iovNum  = iovRem;
       iovLen  = layout.sockLen;
       endLen  = 0;
       if (layout.lLen)
          {ioVec[iovNum-1].iov_len = layout.lLen;
           fixSRD = iovNum-1;
          }
      }
   iovRem -= iovNum;
   lenLeft = layout.sockLen - iovLen;

// Reset remaining fields
//
   boReset();
   info.offset = htonll(fOffs);
   return 0;
}
