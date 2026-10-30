/******************************************************************************/
/*                                                                            */
/*              X r d C k s C o m b i n e T e s t s . c c                     */
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

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <zlib.h>

#include "XrdCks/XrdCksCalccrc32.hh"
#include "XrdCks/XrdCksCalccrc32C.hh"
#include "XrdOuc/XrdOucCRC32C.hh"

namespace
{
using Digest = std::array<unsigned char, 4>;

//------------------------------------------------------------------------------
// Deterministic test data, so that a failure is reproducible.
//------------------------------------------------------------------------------

std::vector<char> TestData(size_t len)
{
   std::vector<char> data(len);
   uint32_t seed = 0x12345678u;

   for (size_t i = 0; i < len; i++)
       {seed = seed * 1103515245u + 12345u;
        data[i] = static_cast<char>((seed >> 16) & 0xff);
       }
   return data;
}

//------------------------------------------------------------------------------
// Copy the four bytes a calculator returns. The pointer refers to storage
// that the next call on the same object overwrites.
//------------------------------------------------------------------------------

Digest Bytes(const char* csVal)
{
   Digest out;
   std::memcpy(out.data(), csVal, out.size());
   return out;
}

//------------------------------------------------------------------------------
// The four bytes of a value in big-endian order. This is what every XRootD
// checksum plugin must emit, whatever the byte order of the host.
//------------------------------------------------------------------------------

Digest BigEndian(uint32_t value)
{
   return Digest{static_cast<unsigned char>((value >> 24) & 0xff),
                 static_cast<unsigned char>((value >> 16) & 0xff),
                 static_cast<unsigned char>((value >>  8) & 0xff),
                 static_cast<unsigned char>( value        & 0xff)};
}

const size_t dataLen  = 4103;  // Crosses a 4096 byte page
const size_t splitOff = 1237;  // An arbitrary interior split point
}

/******************************************************************************/
/*                              c r c 3 2                                     */
/******************************************************************************/

// This test pins the definition of the crc32 digest to the zlib algorithm.
// It fails if the algorithm changes.

TEST(XrdCksCalccrc32Test, FinalIsBigEndianZlibCrc32)
{
   const std::vector<char> data = TestData(dataLen);
   const uint32_t expect = crc32(crc32(0L, Z_NULL, 0),
                                 (const Bytef*)data.data(), data.size());

   XrdCksCalccrc32 calc;
   EXPECT_EQ(Bytes(calc.Calc(data.data(), data.size())), BigEndian(expect));
}

TEST(XrdCksCalccrc32Test, IsCombinable)
{
   XrdCksCalccrc32 calc;
   int csSz = 0;

   EXPECT_TRUE(calc.Combinable());
   EXPECT_STREQ(calc.Type(csSz), "crc32");
   EXPECT_EQ(csSz, 4);
}

TEST(XrdCksCalccrc32Test, CombineIntoCurrentMatchesWhole)
{
   const std::vector<char> data = TestData(dataLen);
   const size_t tailLen = data.size() - splitOff;

   XrdCksCalccrc32 whole, head, tail;
   const Digest expect = Bytes(whole.Calc(data.data(), data.size()));
   const Digest tailCS = Bytes(tail.Calc(data.data()+splitOff, tailLen));

   head.Update(data.data(), splitOff);

   EXPECT_EQ(Bytes(head.Combine((const char*)tailCS.data(), tailLen)), expect);
   EXPECT_EQ(Bytes(head.Final()), expect);
}

TEST(XrdCksCalccrc32Test, CombineTwoMatchesWhole)
{
   const std::vector<char> data = TestData(dataLen);
   const size_t tailLen = data.size() - splitOff;

   XrdCksCalccrc32 whole, head, tail;
   const Digest expect = Bytes(whole.Calc(data.data(), data.size()));
   const Digest headCS = Bytes(head.Calc(data.data(), splitOff));
   const Digest tailCS = Bytes(tail.Calc(data.data()+splitOff, tailLen));

   XrdCksCalccrc32 calc;
   EXPECT_EQ(Bytes(calc.Combine((const char*)headCS.data(),
                                (const char*)tailCS.data(), tailLen)), expect);
}

/******************************************************************************/
/*                             c r c 3 2 c                                    */
/******************************************************************************/

TEST(XrdCksCalccrc32CTest, FinalIsBigEndianCrc32C)
{
   const std::vector<char> data = TestData(dataLen);
   const uint32_t expect = crc32c(0, data.data(), data.size());

   XrdCksCalccrc32C calc;
   EXPECT_EQ(Bytes(calc.Calc(data.data(), data.size())), BigEndian(expect));
}

TEST(XrdCksCalccrc32CTest, IsCombinable)
{
   XrdCksCalccrc32C calc;
   int csSz = 0;

   EXPECT_TRUE(calc.Combinable());
   EXPECT_STREQ(calc.Type(csSz), "crc32c");
   EXPECT_EQ(csSz, 4);
}

TEST(XrdCksCalccrc32CTest, CombineIntoCurrentMatchesWhole)
{
   const std::vector<char> data = TestData(dataLen);
   const size_t tailLen = data.size() - splitOff;

   XrdCksCalccrc32C whole, head, tail;
   const Digest expect = Bytes(whole.Calc(data.data(), data.size()));
   const Digest tailCS = Bytes(tail.Calc(data.data()+splitOff, tailLen));

   head.Update(data.data(), splitOff);

   EXPECT_EQ(Bytes(head.Combine((const char*)tailCS.data(), tailLen)), expect);
   EXPECT_EQ(Bytes(head.Final()), expect);
}

TEST(XrdCksCalccrc32CTest, CombineTwoMatchesWhole)
{
   const std::vector<char> data = TestData(dataLen);
   const size_t tailLen = data.size() - splitOff;

   XrdCksCalccrc32C whole, head, tail;
   const Digest expect = Bytes(whole.Calc(data.data(), data.size()));
   const Digest headCS = Bytes(head.Calc(data.data(), splitOff));
   const Digest tailCS = Bytes(tail.Calc(data.data()+splitOff, tailLen));

   XrdCksCalccrc32C calc;
   EXPECT_EQ(Bytes(calc.Combine((const char*)headCS.data(),
                                (const char*)tailCS.data(), tailLen)), expect);
}

/******************************************************************************/
/*             T h r e e   s e g m e n t   c o m b i n a t i o n              */
/******************************************************************************/

// XrdOfsCksFile combines one segment at a time as writes fill the holes.
// This repeats that sequence.

template<class Calc>
static void CombineThreeSegments()
{
   const std::vector<char> data = TestData(dataLen);
   const size_t seg1 = 128, seg2 = 1024, seg3 = dataLen - seg1 - seg2;

   Calc whole, part;
   const Digest expect = Bytes(whole.Calc(data.data(), data.size()));

   const Digest cs2 = Bytes(part.Calc(data.data()+seg1, seg2));
   const Digest cs3 = Bytes(part.Calc(data.data()+seg1+seg2, seg3));

   Calc calc;
   calc.Update(data.data(), seg1);
   calc.Combine((const char*)cs2.data(), seg2);
   calc.Combine((const char*)cs3.data(), seg3);

   EXPECT_EQ(Bytes(calc.Final()), expect);
}

TEST(XrdCksCalccrc32Test,  CombineThreeSegments)
{
   CombineThreeSegments<XrdCksCalccrc32>();
}

TEST(XrdCksCalccrc32CTest, CombineThreeSegments)
{
   CombineThreeSegments<XrdCksCalccrc32C>();
}
