#ifndef _XRDOSSCSICRCUTILS_H
#define _XRDOSSCSICRCUTILS_H
/******************************************************************************/
/*                                                                            */
/*                X r d O s s C s i C r c U t i l s . h h                     */
/*                                                                            */
/* (C) Copyright 2021 CERN.                                                   */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* In applying this licence, CERN does not waive the privileges and           */
/* immunities granted to it by virtue of its status as an Intergovernmental   */
/* Organization or submit itself to any jurisdiction.                         */
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

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "assert.h"

class XrdOssCsiCrcUtils {
public:

   // crc32c_combine
   //
   // crc1: crc32c value of data1
   // crc2: crc32c value of data2
   // len2: length of data2
   //
   // returns crc of concatenation of data1|data2
   // note: using Calc32C: some optimisation could be made
   static uint32_t crc32c_combine(uint32_t crc1, uint32_t crc2, size_t len2)
   {
      if (len2==0)
         return crc1;

      assert(len2<=XrdSys::PageSize);

      const uint32_t c1 = XrdOucCRC::Calc32C(g_bz, len2, ~crc1);
      return ~c1^crc2;
   }

   // crc32c_split1
   //
   // crctot: crc32c of data1|data2
   // crc2:   crc32c of data2
   // len2:   length of data2
   //
   // returns crc of data1
   // note: crc bitshift to right, significant optimisation likely
   //       possible with intrinsics or a precomputed table
   static uint32_t crc32c_split1(uint32_t crctot, uint32_t crc2, size_t len2)
   {
      if (len2==0)
         return crctot;

      assert(len2<=XrdSys::PageSize);
      uint32_t crc = (crctot ^ crc2);
      for(size_t i=0;i<8*len2;i++) {
         crc = (crc<<1)^((crc&0x80000000) ? (CrcPoly << 1 | 0x1) : 0);
      }
      return crc;
   }

   // crc32c_split2
   //
   // crctot: crc32c of data1|data2
   // crc1:   crc32c of data1
   // len2:   length of data2
   //
   // returns crc of data2
   // note: using Calc32C: some optimisation could be made
   static uint32_t crc32c_split2(uint32_t crctot, uint32_t crc1, size_t len2)
   {
      if (len2==0)
         return 0;

      assert(len2<=XrdSys::PageSize);
      uint32_t c1 = XrdOucCRC::Calc32C(g_bz, len2, ~crc1);
      return ~c1^crctot;
   }

   // crc32c_extendwith_zero
   //
   // crc: crc32c of data
   // len: number of zero bytes to append
   //
   // returns crc of data|[0x00 x len]
   // note: using Calc32C: some optimisation could be made
   static uint32_t crc32c_extendwith_zero(uint32_t crc, size_t len)
   {
      if (len==0)
         return crc;

      assert(len<=XrdSys::PageSize);
      return XrdOucCRC::Calc32C(g_bz, len, crc);
   }

private:

   static const uint8_t g_bz[XrdSys::PageSize];

   // CRC-32C (iSCSI) polynomial in reversed bit order.
   static const uint32_t CrcPoly = 0x82F63B78;
};

#endif
