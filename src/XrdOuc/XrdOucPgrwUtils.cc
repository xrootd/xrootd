/******************************************************************************/
/*                                                                            */
/*                    X r d O u c P g r w U t i l s . c c                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <limits.h>

#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace
{
static const int pgPageMask = XrdProto::kXR_pgPageSZ-1;
static const int pgPageSize = XrdProto::kXR_pgPageSZ;
static const int pgCsumSize = sizeof(uint32_t);
static const int pgUnitSize = XrdProto::kXR_pgPageSZ + pgCsumSize;
static const int pgMaxBSize = INT_MAX & ~pgPageMask;
}

/******************************************************************************/
/*                                c s C a l c                                 */
/******************************************************************************/
  
void XrdOucPgrwUtils::csCalc(const char* data, off_t offs, size_t count,
                             uint32_t* csval)
{
   int pgOff = offs & pgPageMask;

// If this is unaligned, the we must compute the checksum of the leading bytes
// to align them to the next page boundary if one exists.
//
   if (pgOff)
      {size_t chkLen = pgPageSize - pgOff;
       if (chkLen >= count) {chkLen = count; count = 0;}
          else count -= chkLen;
       *csval++ = XrdOucCRC::Calc32C((void *)data, chkLen);
       data += chkLen;
      }

// Compute the remaining checksums, if any are left
//
   if (count) XrdOucCRC::Calc32C((void *)data, count, csval);
}

/******************************************************************************/
  
void XrdOucPgrwUtils::csCalc(const char* data, off_t offs, size_t count,
                             std::vector<uint32_t> &csvec)
{
   int pgOff = offs & pgPageMask;
   int n = XrdOucPgrwUtils::csNum(offs, count);

// Size the vector to be of correct size
//
   csvec.resize(n);
   csvec.assign(n, 0);
   uint32_t *csval = csvec.data();

// If this is unaligned, the we must compute the checksum of the leading bytes
// to align them to the next page boundary if one exists.
//
   if (pgOff)
      {size_t chkLen = pgPageSize - pgOff;
       if (chkLen >= count) {chkLen = count; count = 0;}
          else count -= chkLen;
       *csval++ = XrdOucCRC::Calc32C((void *)data, chkLen);
       data += chkLen;
      }

// Compute the remaining checksums, if any are left
//
   if (count) XrdOucCRC::Calc32C((void *)data, count, csval);
}

/******************************************************************************/
/*                                 c s N u m                                  */
/******************************************************************************/
  
int XrdOucPgrwUtils::csNum(off_t offs, int count)
{
   int k, pgOff = offs & pgPageMask;

// Account for unaligned start
//
   if (!pgOff) k = 0;
      else {int chkLen = pgPageSize - pgOff;
            if (chkLen >= count) return 1;
            count -= chkLen;
            k = 1;
           }

// Return the number of checksum required or will be generated.
//
   return k + count/pgPageSize + ((count & pgPageMask) != 0);
}
  
/******************************************************************************/
  
int XrdOucPgrwUtils::csNum(off_t offs, int count, int &fLen, int &lLen)
{
   int pgOff = offs & pgPageMask;

// Gaurd against invalid input
//
   if (!count)
      {fLen = lLen = 0;
       return 0;
      }

// Account for unaligned start
//
   if (!pgOff) fLen = (pgPageSize <= (int)count ? pgPageSize : count);
      else {fLen = pgPageSize - pgOff;
            if (fLen >= count) fLen = count;
           }

// Compute length of last segement and return number of checksums required
//
   count -= fLen;
   if (count)
      {pgOff = count & pgPageMask;
       lLen  = (pgOff ? pgOff : pgPageSize);
       return 1 + count/pgPageSize + (pgOff != 0);
      }

// There is only one checksum and the last length is the same as the first
//
   lLen = fLen;
   return 1;
}

/******************************************************************************/
/*                                 c s V e r                                  */
/******************************************************************************/
  
bool XrdOucPgrwUtils::csVer(dataInfo &dInfo, off_t &bado, int &badc)
{
   int pgOff = dInfo.offs & pgPageMask;

// Make sure we have something to do
//
   if (dInfo.count <= 0) return true;

// If this is unaligned, the we must verify the checksum of the leading bytes
// to align them to the next page boundary if one exists.
//
   if (pgOff)
      {off_t tempsave;
       int chkLen = pgPageSize - pgOff;
       if (dInfo.count < chkLen) {chkLen = dInfo.count; dInfo.count = 0;}
          else dInfo.count -= chkLen;

       bool aOK = XrdOucCRC::Ver32C((void *)dInfo.data, chkLen, dInfo.csval[0]);

       dInfo.data += chkLen;
       tempsave    = dInfo.offs;
       dInfo.offs += chkLen;
       dInfo.csval++;

       if (!aOK)
          {bado = tempsave;
           badc = chkLen;
           return false;
          }
      }

// Verify the remaining checksums, if any are left (offset is page aligned)
//
   if (dInfo.count > 0)
      {uint32_t valcs;
       int pgNum = XrdOucCRC::Ver32C((void *)dInfo.data,  dInfo.count,
                                             dInfo.csval, valcs);
       if (pgNum >= 0)
          {bado = dInfo.offs + (pgPageSize * pgNum);
           int xlen = (bado - dInfo.offs);
           dInfo.offs  += xlen;
           dInfo.count -= xlen;
           badc = (dInfo.count <= pgPageSize ? dInfo.count : pgPageSize);
           dInfo.offs  += badc;
           dInfo.count -= badc;
           dInfo.csval += (pgNum+1);
           return false;
          }
      }

// All sent well
//
   return true;
}

/******************************************************************************/
/*                            r e c v L a y o u t                             */
/******************************************************************************/

int XrdOucPgrwUtils::recvLayout(Layout &layout, off_t  offs, int dlen, int bsz)
{
   int csNum, dataLen, maxLen;

// Make sure length is correct
//
   if (dlen <= pgCsumSize)
      {layout.eWhy = "invalid length";
       return 0;
      }

// Either validate the bsz or compute a virtual bsz
//
   if (bsz <= 0) bsz = pgMaxBSize;
      else if (bsz & pgPageMask)
              {layout.eWhy = "invalid buffer size (logic error)";
               return 0;
              }
  
// Compute the data length of this request and set initial buffer pointer. While
// the layout should have been verified before we goot here we will return an
// error should something be amiss.
//
   dlen -= pgCsumSize;
   if ((layout.bOffset = offs & pgPageMask))
      {dataLen = pgPageSize - layout.bOffset;
       csNum = 1;
       if (dlen <= dataLen)
          {dataLen = dlen;
           maxLen  = 0;
          } else {
           dlen  -= dataLen;
           maxLen = bsz - pgPageSize;
          }
       layout.fLen = dataLen;
       layout.lLen = 0;
      } else {
       if (dlen <= pgPageSize)
          {dataLen = layout.fLen = dlen;
           layout.lLen = 0;
           maxLen  = 0;
           csNum   = 1;
          } else {
           dlen   += pgCsumSize;
           dataLen = 0;
           maxLen  = bsz;
           csNum   = 0;
           layout.fLen = pgPageSize;
          }
      }

// Compute the length without the checksums and the maximum data bytes to read
// And the number of checksums we will have.
//
   if (maxLen)
      {int bytes = dlen / pgUnitSize * pgPageSize;
       int bfrag = dlen % pgUnitSize;
       if (bfrag)
          {if (bfrag <= pgCsumSize)
               {layout.eWhy = "last page too short";
                return 0;
               }
           bytes += bfrag - pgCsumSize;
          }
       if (bytes > maxLen) bytes = maxLen;
       dataLen += bytes;
       layout.lLen = bytes & pgPageMask;
       csNum   += bytes/pgPageSize + (layout.lLen != 0);
       if (layout.lLen == 0) layout.lLen = pgPageSize;
      }

// Set layout data bytes and sock bytes and return the number of checksums
//
   layout.dataLen = dataLen;
   layout.sockLen = dataLen + (csNum * pgCsumSize);
   layout.eWhy    = 0;
   return csNum;
}

/******************************************************************************/
/*                            s e n d L a y o u t                             */
/******************************************************************************/

int XrdOucPgrwUtils::sendLayout(Layout &layout, off_t offs, int dlen, int bsz)
{
   int csNum, pgOff = offs & pgPageMask;

// Make sure length is correct
//
   if (dlen <= 0)
      {layout.eWhy = "invalid length";
       return 0;
      }

// Either validate the bsz or compute a virtual bsz
//
   if (bsz <= 0) bsz = pgMaxBSize;
      else if (bsz & pgPageMask)
              {layout.eWhy = "invalid buffer size (logic error)";
               return 0;
              }
   layout.eWhy = 0;

// Account for unaligned start
//
   if (!pgOff) layout.fLen = (pgPageSize <= dlen ? pgPageSize : dlen);
      else {layout.fLen = pgPageSize - pgOff;
            if (layout.fLen > dlen) layout.fLen = dlen;
           }
   layout.bOffset = pgOff;

// Adjust remaining length and reduce the buffer size as we have effectively
// used the first page of the buffer.
//
   bsz  -= pgPageSize;
   dlen -= layout.fLen;

// Compute length of last segement and compute number of checksums required
//
   if (dlen && bsz)
      {if (dlen > bsz) dlen = bsz;
       if ((pgOff = dlen & pgPageMask)) layout.lLen = pgOff;
          else layout.lLen = (pgPageSize <= dlen ? pgPageSize : dlen);
       csNum = 1 + dlen/pgPageSize + (pgOff != 0);
       layout.dataLen = layout.fLen + dlen;
      } else {
       csNum = 1;
       layout.lLen = 0;
       layout.dataLen = layout.fLen;
      }

// Set network bytes and return number of checksumss the same as the first
//
   layout.sockLen = layout.dataLen + (csNum * pgCsumSize);
   return csNum;
}
