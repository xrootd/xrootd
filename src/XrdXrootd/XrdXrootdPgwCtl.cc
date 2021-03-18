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

#include <string.h>

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdPgwCtl.hh"
#include "XrdXrootd/XrdXrootdPgwFob.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

#define TRACELINK this

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdOucTrace *XrdXrootdTrace;

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
const char *XrdXrootdPgwCtl::TraceID = "pgwCtl";

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdPgwCtl::XrdXrootdPgwCtl(const char *id, int pid)
                : ID(id), dataBuff(0), dataBLen(0), boCount(0),
                  fixSRD(0), pathID(pid), isSusp(false)
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
        ioVec[i+1].iov_len  = XrdProto::kXR_pgPageSZ;
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
   ioVec[1].iov_len  = XrdProto::kXR_pgPageSZ;

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
   if (n > 1) iovLen += (n-1)*XrdProto::kXR_pgPageSZ;

// Indicate there is more to do
//
   return true;
}

/******************************************************************************/
/*                                 b o A d d                                  */
/******************************************************************************/

const char *XrdXrootdPgwCtl::boAdd(XrdXrootdFile *fP, kXR_int64 foffs, int dlen)
{

// Do some tracing
//
   TRACEI(PGWR, int(pathID) <<" csErr "<<dlen<<'@'<<foffs<<" inreq="<<boCount+1
                <<" infile=" <<fP->pgwFob->numOffs()+1<<" fn="<<fP->FileKey);

// If this is the first offset, record the length as first and last.
// Othewrise just update the last length.
//
   if (!boCount) cse.dlFirst = cse.dlLast = htons(dlen);
      else cse.dlLast = htons(dlen);

// Add offset to the vector to be returned to client for corrections.
//
   if (boCount+1 >= XrdProto::kXR_pgMaxEpr)
      return "Too many checksum errors in request";
   badOffs[boCount++] = htonll(foffs);

// Add offset in the set of uncorrected offsets
//
   if (!fP->pgwFob->addOffs(foffs, dlen))
      return "Too many uncorrected checksum errors in file";

// Success!
//
   return 0;
}
  
/******************************************************************************/
/*                                b o I n f o                                 */
/******************************************************************************/
  
char *XrdXrootdPgwCtl::boInfo(int &boLen)
{

// If no bad offsets are present, indicate so.
//
   if (!boCount)
      {boLen = 0;
       return 0;
      }

// Return the additional data
//
   boLen = sizeof(cse) + (boCount * sizeof(kXR_int64));
   cse.cseCRC = htonl(XrdOucCRC::Calc32C(((char *)&cse)+crcSZ, boLen-crcSZ));
   return (char *)&cse;
}
  
/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
const char *XrdXrootdPgwCtl::Setup(XrdBuffer *buffP, kXR_int64 fOffs, int totlen)
{
   int fsLen, pgOff, iovMax, units;

// Reset short length in the iovec from the last use.
//
   if (fixSRD)
      {ioVec[fixSRD].iov_len = XrdProto::kXR_pgPageSZ;
       fixSRD = 0;
      }

// Make sure the first segment is not too short
//
   if (totlen <= crcSZ) return "pgwrite length is too short";

// Compute the maximum number of iov entries we can use relative to buffer size
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

// If the offset is misaligned, compute the implied length of the first segment
// Note that we already gauranteed totlen is > crcSZ (i.e. 5 or more bytes).
//
   pgOff = fOffs & (XrdProto::kXR_pgPageSZ-1);
   if (pgOff == 0)
      {fsLen = 0;
       units = 0;
       ioVec[1].iov_base = dataBuff;
       ioVec[1].iov_len  = XrdProto::kXR_pgPageSZ;
      } else {
       fsLen = XrdProto::kXR_pgPageSZ - pgOff;
       units = 1;
       if (totlen - crcSZ > fsLen) totlen = totlen - (fsLen + crcSZ);
          else {fsLen  = totlen - crcSZ;
                totlen = 0;
                fixSRD = 1;
               }
       ioVec[1].iov_base = dataBuff + pgOff;
       ioVec[1].iov_len  = fsLen;
      }

// Compute the length of the last segment and adjust the units. Note that
// we make sure that the last segment has a checksum and at least one byte
// of data; otherwise, we return an error.
//
   if (totlen && ((fsLen = totlen % XrdProto::kXR_pgUnitSZ)))
      {if (fsLen <= (int)sizeof(kXR_unt32))
          return "pgwrite last segment too short";
       units++;
       endLen = fsLen - crcSZ;
      } else endLen = 0;

// Compute the total iovec elements that we will need to handle this request
//
   iovRem = (units + totlen/XrdProto::kXR_pgUnitSZ) * 2;

// Compute how many iovec elements we can issue per read and adjust remaining.
//
   if (iovRem > iovMax) iovNum = iovMax;
      else {iovNum = iovRem;
            if (endLen)
               {ioVec[iovNum-1].iov_len = endLen;
                fixSRD = iovNum-1;
               }
           }
   iovRem -= iovNum;

// Set the read length of this ioVec
//
   units  = iovNum>>1;
   iovLen = ioVec[1].iov_len + (units*crcSZ);
   if (units > 1)
      {iovLen += ioVec[iovNum-1].iov_len;
       if (units > 2) iovLen += (units-2)*XrdProto::kXR_pgPageSZ;
      }

// Reset remaining fields
//
   boCount = 0;
   info.offset = htonll(fOffs);
   return 0;
}
