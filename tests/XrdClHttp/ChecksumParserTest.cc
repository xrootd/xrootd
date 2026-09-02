/******************************************************************************/
/* Copyright (C) 2026 by European Organization for Nuclear Research (CERN)   */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/******************************************************************************/

#include "XrdClHttp/XrdClHttpUtil.hh"

#include <gtest/gtest.h>

namespace {

std::string HexValue(const XrdClHttp::ChecksumInfo &info,
                     XrdClHttp::ChecksumType type) {
    const auto &value = info.Get(type);
    static const char digits[] = "0123456789abcdef";
    std::string result;
    for (std::size_t index = 0; index < XrdClHttp::GetChecksumLength(type);
         ++index) {
        result += digits[value[index] >> 4];
        result += digits[value[index] & 0x0f];
    }
    return result;
}

}

TEST(HttpChecksum, MapsAdler32Type) {
    EXPECT_EQ(XrdClHttp::GetTypeFromString("adler32"),
              XrdClHttp::ChecksumType::kADLER32);
    EXPECT_EQ(XrdClHttp::GetTypeFromString("ADLER32"),
              XrdClHttp::ChecksumType::kADLER32);
    EXPECT_EQ(XrdClHttp::GetTypeFromString("AdLeR32"),
              XrdClHttp::ChecksumType::kADLER32);
    EXPECT_EQ(XrdClHttp::HeaderParser::ChecksumTypeToDigestName(
                  XrdClHttp::ChecksumType::kADLER32),
              "adler32");
}

TEST(HttpChecksum, ParsesAdler32Digest) {
    struct TestCase {
        const char *digest;
        const char *expected;
    };
    const TestCase cases[] = {
        {"adler32=1", "00000001"},
        {"ADLER32=35E754F", "035e754f"},
        {"AdLeR32=335E754F", "335e754f"}
    };

    for (const auto &test : cases) {
        XrdClHttp::ChecksumInfo info;
        XrdClHttp::HeaderParser::ParseDigest(test.digest, info);
        ASSERT_TRUE(info.IsSet(XrdClHttp::ChecksumType::kADLER32))
            << test.digest;
        EXPECT_EQ(HexValue(info, XrdClHttp::ChecksumType::kADLER32),
                  test.expected);
    }
}

TEST(HttpChecksum, ParsesCommaSeparatedDigestWithWhitespace) {
    XrdClHttp::ChecksumInfo info;
    XrdClHttp::HeaderParser::ParseDigest(
        " MD5=1B2M2Y8AsgTpgAmY7PhCfg== , ADLER32=335E754F ", info);

    ASSERT_TRUE(info.IsSet(XrdClHttp::ChecksumType::kADLER32));
    EXPECT_EQ(HexValue(info, XrdClHttp::ChecksumType::kADLER32), "335e754f");
    EXPECT_TRUE(info.IsSet(XrdClHttp::ChecksumType::kMD5));
}

TEST(HttpChecksum, RejectsInvalidAdler32Digest) {
    const char *invalid[] = {
        "adler32", "adler32=", "adler32=-1", "adler32=+1", "adler32=0x1",
        "adler32=123456789", "adler32=not-hex"
    };

    for (const char *digest : invalid) {
        XrdClHttp::ChecksumInfo info;
        XrdClHttp::HeaderParser::ParseDigest(digest, info);
        EXPECT_FALSE(info.IsSet(XrdClHttp::ChecksumType::kADLER32)) << digest;
    }
}

TEST(HttpChecksum, AppliesStrictHexParsingToCrc32c) {
    XrdClHttp::ChecksumInfo info;
    XrdClHttp::HeaderParser::ParseDigest("crc32c=-1", info);
    EXPECT_FALSE(info.IsSet(XrdClHttp::ChecksumType::kCRC32C));
}
