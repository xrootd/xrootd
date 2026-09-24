//------------------------------------------------------------------------------
// Tests for the GSI random tag helpers, see src/XrdSecgsi/XrdSecgsiRtag.hh
//------------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <string>

#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSecgsi/XrdSecgsiRtag.hh"
#include "XrdSut/XrdSutRndm.hh"

namespace {

// The 51 byte SHA-256 DigestInfo an attacker would ask the server to sign
const unsigned char kDigestInfo[51] = {
   0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
   0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20, 0xc3, 0x7e, 0xf7, 0x9d, 0x95,
   0x5b, 0x61, 0x6b, 0xf9, 0x09, 0x89, 0x34, 0x8a, 0x45, 0x42, 0xc7, 0xfe,
   0xda, 0x92, 0xe8, 0x0e, 0x7b, 0x2d, 0x84, 0xf2, 0x6f, 0x7a, 0x3d, 0xd4,
   0x3b, 0x55, 0x39 };

// SHA-256("XRD-GSI-RTAG-v1" || "abcdefgh")
const char kSha256OfCtxAbcdefgh[] =
   "52df717c0c4b2a41dd6f047b8db48cb08677239460bca9d556b34ea7bc6762d0";

std::string ToHex(const char *b, int l)
{
   static const char hex[] = "0123456789abcdef";
   std::string s;
   for (int i = 0; i < l; i++) {
      s += hex[(unsigned char)b[i] >> 4];
      s += hex[(unsigned char)b[i] & 0xf];
   }
   return s;
}

} // namespace

TEST(XrdSecgsiRtag, GeneratedTagIsValid)
{
   XrdOucString rtag;
   ASSERT_EQ(XrdSutRndm::GetRndmTag(rtag), 0);
   EXPECT_EQ(rtag.length(), kXRSrtagLen);
   EXPECT_TRUE(XrdSecgsiRtagIsValid(rtag.c_str(), rtag.length()));
}

TEST(XrdSecgsiRtag, DigestInfoIsRejected)
{
   EXPECT_FALSE(XrdSecgsiRtagIsValid((const char *) kDigestInfo,
                                     sizeof(kDigestInfo)));
}

TEST(XrdSecgsiRtag, MalformedTagsAreRejected)
{
   EXPECT_FALSE(XrdSecgsiRtagIsValid(nullptr, kXRSrtagLen));
   EXPECT_FALSE(XrdSecgsiRtagIsValid("abcdefg", 7));   // too short
   EXPECT_FALSE(XrdSecgsiRtagIsValid("abcdefghi", 9)); // too long
   EXPECT_FALSE(XrdSecgsiRtagIsValid("abcdef g", kXRSrtagLen)); // space
   EXPECT_FALSE(XrdSecgsiRtagIsValid("abcdef\0g", kXRSrtagLen)); // NUL
}

TEST(XrdSecgsiRtag, DigestIsContextBoundSha256)
{
   XrdCryptoFactory *cf = XrdCryptoFactory::GetCryptoFactory("ssl");
   ASSERT_NE(cf, nullptr);

   char md[kXRSrtagMDMax] = {0};
   const char rtag[] = "abcdefgh";

   const int lmd = XrdSecgsiRtagDigest(cf, rtag, kXRSrtagLen, md,
                                       kXRSrtagMDMax);
   ASSERT_EQ(lmd, 32);

   // SHA-256 of "XRD-GSI-RTAG-v1abcdefgh"
   EXPECT_EQ(ToHex(md, lmd), kSha256OfCtxAbcdefgh);
}

TEST(XrdSecgsiRtag, DigestRejectsBadInput)
{
   XrdCryptoFactory *cf = XrdCryptoFactory::GetCryptoFactory("ssl");
   ASSERT_NE(cf, nullptr);

   char md[kXRSrtagMDMax] = {0};
   EXPECT_EQ(XrdSecgsiRtagDigest(nullptr, "abcdefgh", 8, md, kXRSrtagMDMax), -1);
   EXPECT_EQ(XrdSecgsiRtagDigest(cf, nullptr, 8, md, kXRSrtagMDMax), -1);
   EXPECT_EQ(XrdSecgsiRtagDigest(cf, "abcdefgh", 0, md, kXRSrtagMDMax), -1);
   EXPECT_EQ(XrdSecgsiRtagDigest(cf, "abcdefgh", 8, md, 8), -1); // too small
}
