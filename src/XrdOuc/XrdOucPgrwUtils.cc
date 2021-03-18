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

#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace
{
static const int pgPageSize = XrdProto::kXR_pgPageSZ;
static const int pgPageMask = XrdProto::kXR_pgPageSZ-1;
}

/******************************************************************************/
/*                                c s C a l c                                 */
/******************************************************************************/
  
void XrdOucPgrwUtils::csCalc(const char* data, ssize_t offs, size_t count,
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
/*                                 c s N u m                                  */
/******************************************************************************/
  
int XrdOucPgrwUtils::csNum(ssize_t offs, size_t count)
{
   int k, pgOff = offs & pgPageMask;

// Account for unaligned start
//
   if (!pgOff) k = 0;
      else {size_t chkLen = pgPageSize - pgOff;
            if (chkLen >= count) return 1;
            count -= chkLen;
            k = 1;
           }

// Return the number of checksum required or will be generated.
//
   return k + count/pgPageSize + ((count & pgPageMask) != 0);
}
  
/******************************************************************************/
/*                                 c s V e r                                  */
/******************************************************************************/
  
int XrdOucPgrwUtils::csVer(const char*     data,  ssize_t  offs, size_t  count,
                           const uint32_t* csval, ssize_t &bado, size_t &badc)
{
   int k = 0, pgOff = offs & pgPageMask;

// If this is unaligned, the we must verify the checksum of the leading bytes
// to align them to the next page boundary if one exists.
//
   if (pgOff)
      {size_t chkLen = pgPageSize - pgOff;
       if (count < chkLen) {chkLen = count; count = 0;}
          else count -= chkLen;
       if (!XrdOucCRC::Ver32C((void *)data, chkLen, csval[0]))
          {bado = offs;
           badc = chkLen;
           return 1;
          }
       data += chkLen;
       offs += chkLen;
       k = 1;
      }

// Verify the remaining checksums, if any are left
//
   if (count)
      {uint32_t valcs;
       int pgNum = XrdOucCRC::Ver32C((void *)data, count, &csval[k], valcs);
       if (pgNum >= 0)
          {bado = offs + (pgPageSize * pgNum);
           if (pgNum < (csNum(offs, count) - 1)) badc = pgPageSize;
              else {int pFrag = count & pgPageMask;
                    badc = (pFrag ? pFrag : pgPageSize);
                   }
           return k + pgNum + 1;
          }
      }

// All sent well
//
   return 0;
}
