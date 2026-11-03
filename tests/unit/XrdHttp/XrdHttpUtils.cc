#undef NDEBUG

#include "XrdHttp/XrdHttpUtils.hh"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace testing;

static inline const std::pair<std::string,std::string> encodedDecodedStrings [] {
  {"zteos64%3AMDAF5PGJ4Wa12g%3D","zteos64:MDAF5PGJ4Wa12g="},
  //"zteos64%3BAMDAF5PGJ4Wa12g%3B%3B",
  {"xrootd_tpc-helloworld","xrootd_tpc-helloworld"},
  {"",""},
  //"zteos64:%%test",
  {"Bearer%20token","Bearer token"},
  {"%20%5B%5D%3A%23%3D%0A%0D"," []:#=\n\r"}
};

TEST(UtilsTest, EncodeStrAndDecodeStrConvertBothWays) {
  for(auto [encoded,decoded]: encodedDecodedStrings) {
    ASSERT_EQ(decode_str(encoded), decoded);
    ASSERT_EQ(encoded, encode_str(decoded));
    ASSERT_EQ(encode_str(decode_str(encoded)), encoded);
    ASSERT_EQ(decode_str(encode_str(decoded)), decoded);
  }
}

static inline const std::pair<std::string,std::string> decodedEncodedOpaque [] {
  {"",""},
  {"authz=Bearer token","authz=Bearer%20token"},
  {"test=test1&authz=Bearer token","test=test1&authz=Bearer%20token"},
  {"test=","test="},
  {"test=&authz=Bearer token&authz2=[]","test=&authz=Bearer%20token&authz2=%5B%5D"},
  // CRLF smuggled across the '=' boundary must not survive into the
  // encoded output (otherwise it would land verbatim in a Location:
  // header and split the response).
  {"\r\nSet-Cookie: pwn=1","%0D%0ASet-Cookie%3A%20pwn=1"},
  {"a:b=c","a%3Ab=c"},
  {"foo bar=baz","foo%20bar=baz"}
};

TEST(UtilsTest,EncodeOpaqueEncodesKeysAndValuesButKeepsSeparators) {
  for(auto [decoded,encoded]: decodedEncodedOpaque) {
    ASSERT_EQ(encoded,encode_opaque(decoded));
  }
}

TEST(UtilsTest, HttpPathFromAbsoluteUrlReturnsThePathOrSlash) {
  EXPECT_EQ("/store/a", httpPathFromAbsoluteUrl("https://site.example:1094/store/a"));
  EXPECT_EQ("//pfn/path", httpPathFromAbsoluteUrl("root://host:1094//pfn/path"));
  // HTTP clients request "/" when the URL has no path.
  EXPECT_EQ("/", httpPathFromAbsoluteUrl("https://site.example"));
  EXPECT_EQ("/", httpPathFromAbsoluteUrl("http://site.example:1094"));
  EXPECT_EQ("/", httpPathFromAbsoluteUrl("site.example"));
  EXPECT_EQ("/", httpPathFromAbsoluteUrl(nullptr));
  // Redir hashes decode_str() then collapse, matching parseResource().
  EXPECT_EQ("/store/a b", httpCollapseSlashes(decode_str(
      httpPathFromAbsoluteUrl("http://host/store//a%20b"))));
}

TEST(UtilsTest, HttpBuildRedirectLocationJoinsSchemeHostPortAndPath) {
  EXPECT_EQ("Location: https://site.example:1094/store/lfn",
            httpBuildRedirectLocation(true, 1094, "site.example", "/store/lfn"));
  EXPECT_EQ("Location: http://site.example:1094/store/lfn",
            httpBuildRedirectLocation(false, 1094, "site.example", "/store/lfn"));
}

TEST(UtilsTest, HttpBuildRedirectLocationKeepsAUrlTargetUnchanged) {
  // port < 0 => URL-form: do not double the scheme or append the original LFN.
  EXPECT_EQ("Location: https://site.example:443//dcache/store/file.root",
            httpBuildRedirectLocation(
                true, -1, "https://site.example:443//dcache/store/file.root", "/store/lfn"));
  // No explicit port, and a '//' in the PFN path, are both preserved verbatim.
  EXPECT_EQ("Location: https://site.example//pfn/file.root",
            httpBuildRedirectLocation(true, -1, "https://site.example//pfn/file.root",
                                      "/store/lfn"));
  // XrdCmsRedirLocal sends localroot + path, so the LFN is already in the URL.
  EXPECT_EQ("Location: file:///data/localroot/store/lfn",
            httpBuildRedirectLocation(false, -1, "file:///data/localroot/store/lfn",
                                      "/store/lfn"));
}

TEST(UtilsTest, HttpCollapseSlashesReplacesRunsOfSlashesWithOne) {
  EXPECT_EQ("/pfn/path", httpCollapseSlashes("//pfn/path"));
  EXPECT_EQ("/a/b", httpCollapseSlashes("///a////b"));
  EXPECT_EQ("/store/plain", httpCollapseSlashes("/store/plain"));
  EXPECT_EQ("/", httpCollapseSlashes("//"));
  EXPECT_EQ("", httpCollapseSlashes(""));

  // decode_str() then collapse is the canonical request path: both ends of a
  // redirect must produce the same string or the token hash will not validate.
  EXPECT_EQ("/store/a b", httpCollapseSlashes(decode_str("/store//a%20b")));
  EXPECT_EQ("/store/a+b", httpCollapseSlashes(decode_str("/store/a%2Bb")));
  // Collapsing does not decode, so it cannot decode a second time.
  EXPECT_EQ("/store/a%20b", httpCollapseSlashes(decode_str("/store/a%2520b")));
}

static inline const std::pair<std::string,std::pair<bool,std::vector<uint8_t>>> hexDigestToBinary [] {
  {"deadbeef", {true,std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}}},
  {"DEADBEEF", {true,std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}}},
  {"00",       {true,std::vector<uint8_t>{0x00}}},
  {"",         {true,std::vector<uint8_t>{}}},
  {"01ff",     {true,std::vector<uint8_t>{0x01, 0xff}}},
  {"xyz",      {false,{}}},  // Invalid input: 'x', 'y', 'z'
  {"1",        {false,{}}},  // Invalid input: odd length
};

TEST(UtilsTest, FromHexDigestConvertsHexToBytesAndRejectsBadInput) {
  for(const auto & [hex,expected]: hexDigestToBinary) {
    std::vector<uint8_t> outputBytes;
    ASSERT_EQ(expected.first,Fromhexdigest(hex,outputBytes));
    ASSERT_EQ(expected.second,outputBytes);
  }
}

static inline const std::pair<std::vector<uint8_t>,std::string> binaryToBase64[] {
  // Original test
  {{0xde, 0xad, 0xbe, 0xef}, "3q2+7w=="},
  // Empty vector --> empty Base64 string
  {{}, ""},
  // Single byte --> 2 padding characters
  {{0x4d}, "TQ=="},             // 'M'
  // Two bytes --> 1 padding character
  {{0x4d, 0x61}, "TWE="},       // 'Ma'
  // Three bytes --> no padding
  {{0x4d, 0x61, 0x6e}, "TWFu"}, // 'Man'
  // Null byte at start
  {{0x00, 0x01, 0x02}, "AAEC"},
  // Null byte in the middle
  {{0x41, 0x00, 0x42}, "QQBC"},
  // Full range from 0x00 to 0x0f
  {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
                             "AAECAwQFBgcICQoLDA0ODw=="},
  // ASCII "hello world"
  {{'h','e','l','l','o',' ','w','o','r','l','d'}, "aGVsbG8gd29ybGQ="},
  // All printable ASCII (truncated to fit in a short test)
  {{'A','B','C','D','E','F','G','H','I','J','K','L'}, "QUJDREVGR0hJSktM"}
};

TEST(UtilsTest,ToBase64EncodesBytesWithPadding) {
  for(const auto & binToBase64: binaryToBase64) {
    std::string base64Output;
    Tobase64(binToBase64.first,base64Output);
    ASSERT_EQ(binToBase64.second,base64Output);
  }
}

TEST(UtilsTest,Base64ToBytesDecodesPaddedBase64) {
  for(const auto & binToBase64: binaryToBase64) {
    std::vector<uint8_t> output;
    base64ToBytes(binToBase64.second,output);
    ASSERT_EQ(binToBase64.first,output);
  }
}

static inline const std::pair<std::string,std::string> b64ToHex[] {
  {"",""},
  {"NVTD2Q==","3554c3d9"},
  {"+l8FpQ==","fa5f05a5"},
};

TEST(UtilsTest, Base64DecodeHexConvertsBase64ToHexAndBack) {
  for(const auto & b64ToH: b64ToHex) {
    std::string output;
    base64DecodeHex(b64ToH.first, output);
    ASSERT_EQ(b64ToH.second,output);
    std::vector<uint8_t> bytes;
    Fromhexdigest(output,bytes);
    Tobase64(bytes,output);
    ASSERT_EQ(b64ToH.first,output);
  }
}
