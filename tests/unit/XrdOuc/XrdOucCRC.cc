/******************************************************************************/
/*                                                                            */
/*                   X r d O u c C R C T e s t s . c c                        */
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
/******************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucCRC32C.hh"

namespace
{
//------------------------------------------------------------------------------
// Deterministic test data, so that a failure is reproducible.
//------------------------------------------------------------------------------

std::vector<char> TestData(size_t len)
{
   std::vector<char> data(len);
   uint32_t seed = 0x9e3779b9u;

   for (size_t i = 0; i < len; i++)
       {seed = seed * 1103515245u + 12345u;
        data[i] = static_cast<char>((seed >> 16) & 0xff);
       }
   return data;
}

const size_t dataLen = 9001;   // Crosses two 4096 byte pages
}

/******************************************************************************/
/*                     c r c 3 2 c _ c o m b i n e                            */
/******************************************************************************/

TEST(Crc32cCombineTest, MatchesWholeAtEverySplit)
{
   const std::vector<char> data = TestData(dataLen);
   const char* p = data.data();
   const uint32_t whole = crc32c(0, p, dataLen);

// The split points cover a one byte head, a one byte tail, and offsets
// on either side of a page boundary.
//
   for (size_t split : {size_t(1), size_t(17), size_t(4095), size_t(4096),
                        size_t(4097), size_t(8192), dataLen-1})
       {const uint32_t csA = crc32c(0, p, split);
        const uint32_t csB = crc32c(0, p+split, dataLen-split);

        EXPECT_EQ(crc32c_combine(csA, csB, dataLen-split), whole)
                 << "split at " << split;
       }
}

TEST(Crc32cCombineTest, EmptySecondBlockReturnsFirst)
{
   const std::vector<char> data = TestData(dataLen);
   const uint32_t csA = crc32c(0, data.data(), dataLen);

   EXPECT_EQ(crc32c_combine(csA, 0, 0), csA);
}

TEST(Crc32cCombineTest, CombineIsAssociative)
{
   const std::vector<char> data = TestData(dataLen);
   const char* p = data.data();
   const size_t a = 1000, b = 3000, c = dataLen - a - b;

   const uint32_t csA = crc32c(0, p,     a);
   const uint32_t csB = crc32c(0, p+a,   b);
   const uint32_t csC = crc32c(0, p+a+b, c);

   const uint32_t left  = crc32c_combine(crc32c_combine(csA, csB, b), csC, c);
   const uint32_t right = crc32c_combine(csA, crc32c_combine(csB, csC, c), b+c);

   EXPECT_EQ(left,  crc32c(0, p, dataLen));
   EXPECT_EQ(right, crc32c(0, p, dataLen));
}

/******************************************************************************/
/*                   X r d O u c C R C : : C o m b i n e 3 2 C                */
/******************************************************************************/

TEST(XrdOucCRCTest, Combine32CAgreesWithCrc32cCombine)
{
   const std::vector<char> data = TestData(dataLen);
   const char* p = data.data();
   const size_t split = 4096;

   const uint32_t csA = XrdOucCRC::Calc32C(p, split);
   const uint32_t csB = XrdOucCRC::Calc32C(p+split, dataLen-split);

   EXPECT_EQ(XrdOucCRC::Combine32C(csA, csB, dataLen-split),
             crc32c_combine(csA, csB, dataLen-split));
   EXPECT_EQ(XrdOucCRC::Combine32C(csA, csB, dataLen-split),
             XrdOucCRC::Calc32C(p, dataLen));
}
