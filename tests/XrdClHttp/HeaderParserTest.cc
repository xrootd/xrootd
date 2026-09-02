/******************************************************************************/
/*                                                                            */
/*              X r d C l H t t p H e a d e r P a r s e r T e s t           */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
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
/******************************************************************************/

#include "XrdClHttp/XrdClHttpUtil.hh"

#include <gtest/gtest.h>

namespace
{
void ExpectSequentialChecksum(const XrdClHttp::ChecksumInfo &checksums,
                              XrdClHttp::ChecksumType type, std::size_t size)
{
  ASSERT_TRUE(checksums.IsSet(type));
  const auto &value = checksums.Get(type);
  for(std::size_t index = 0; index < size; ++index)
    EXPECT_EQ(value[index], index);
}
}

TEST(HeaderParser, RejectsInvalidResponseFieldNames)
{
  XrdClHttp::HeaderParser parser;
  EXPECT_TRUE(parser.Parse("HTTP/1.1 200 OK\r\n"));
  EXPECT_FALSE(parser.Parse("application/type: json\r\n"));
}

TEST(HeaderParser, ParsesSupportedDigestAlgorithms)
{
  XrdClHttp::ChecksumInfo checksums;
  XrdClHttp::HeaderParser::ParseDigest(
    "adler32=00010203, crc32=00010203, CRC32c=AAECAw==, "
    "MD5=AAECAwQFBgcICQoLDA0ODw==, "
    "SHA=AAECAwQFBgcICQoLDA0ODxAREhM=, "
    "SHA-256=AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
    checksums);

  ExpectSequentialChecksum(
    checksums, XrdClHttp::ChecksumType::kADLER32, 4);
  ExpectSequentialChecksum(
    checksums, XrdClHttp::ChecksumType::kCRC32, 4);
  ExpectSequentialChecksum(
    checksums, XrdClHttp::ChecksumType::kCRC32C, 4);
  ExpectSequentialChecksum(checksums, XrdClHttp::ChecksumType::kMD5, 16);
  ExpectSequentialChecksum(checksums, XrdClHttp::ChecksumType::kSHA1, 20);
  ExpectSequentialChecksum(checksums, XrdClHttp::ChecksumType::kSHA256, 32);
}

TEST(HeaderParser, AcceptsAdlerDigestAlias)
{
  XrdClHttp::ChecksumInfo checksums;
  XrdClHttp::HeaderParser::ParseDigest("adler=00010203", checksums);
  ExpectSequentialChecksum(
    checksums, XrdClHttp::ChecksumType::kADLER32, 4);
}

TEST(HeaderParser, BuildsChecksumNegotiationValues)
{
  EXPECT_EQ(XrdClHttp::GetTypeFromString("adler"),
            XrdClHttp::ChecksumType::kADLER32);
  EXPECT_EQ(XrdClHttp::GetTypeFromString("adler32"),
            XrdClHttp::ChecksumType::kADLER32);
  EXPECT_EQ(XrdClHttp::HeaderParser::ChecksumTypeToDigestName(
              XrdClHttp::ChecksumType::kAll),
            "adler32,crc32,CRC32c,MD5,SHA,SHA-256");
}
