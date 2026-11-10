#undef NDEBUG

#include "XrdHttp/XrdHttpHeaderUtils.hh"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <tuple>
#include <utility>

using namespace testing;

static inline const std::pair<std::string, std::map<std::string,std::string>> reprDigest[] {
  {"", {}},
  {"adler=:NVTD2Q==:",{{"adler","3554c3d9"}}},
  {"ADLER=:NVTD2Q==:,sha256=:vykSZ1cmY46OIQzsSuyJkkMG8mxfR1pq+KC05AntFds=:",{{"adler","3554c3d9"},{"sha256","bf2912675726638e8e210cec4aec89924306f26c5f475a6af8a0b4e409ed15db"}}},
  {"adler=",{}},
  {"adler=,sha256=:vykSZ1cmY46OIQzsSuyJkkMG8mxfR1pq+KC05AntFds=:",{{"sha256","bf2912675726638e8e210cec4aec89924306f26c5f475a6af8a0b4e409ed15db"}}},
  {"azerty",{}},
  {"=::value:",{}}
};

TEST(HeaderUtilsTest, ParseReprDigestDecodesBase64ToHexAndSkipsBadEntries) {
  for(const auto & [input, expectedMap]: reprDigest) {
    std::map<std::string,std::string> output;
    XrdHttpHeaderUtils::parseReprDigest(input,output);
    ASSERT_EQ(expectedMap, output);
  }
}

static inline const std::pair<std::string, std::map<std::string, uint8_t>> wantReprDigests[] {
  {"",{}},
  {"adler=1",{{"adler",1}}},
  {"AdLeR=1",{{"adler",1}}},
  {"adler=1, sha-512=2",{{"adler",1},{"sha-512",2}}},
  {"adler=",{}},
  {"adler=azerty",{}},
  {"adler=-1",{}},
  {"adler=11",{{"adler",10}}},
  {"=",{}},
};

TEST(HeaderUtilsTest, ParseWantReprDigestKeepsValidEntriesAndCapsWeightAtTen) {
  for(const auto & [input, expectedMap]: wantReprDigests) {
    std::map<std::string,uint8_t> output;
    XrdHttpHeaderUtils::parseWantReprDigest(input,output);
    ASSERT_EQ(expectedMap, output);
  }
}

// Each test case is (header value, expected parsed result). A non-negative
// expected value means the input is valid and the parser must return that
// exact value; a negative expected value (-1, -2, -3) means the input is
// malformed and the parser must return that exact error code.
static inline const std::tuple<std::string, ssize_t> contentLengthCases[] {
  // Plain numeric values
  {"0",                           0},
  {"5",                           5},
  {"1234567890",                  1234567890},
  // Production format: parseLine forwards the trailing CRLF
  {"5\r\n",                       5},
  {"42\r\n",                      42},
  // Trailing spaces / tabs before the CRLF are tolerated
  {"7 \t\r\n",                    7},
  // Empty / whitespace-only values → -1
  {"",                            -1},
  {"\r\n",                        -1},
  {" \t\r\n",                     -1},
  // Negative, signed, embedded non-digit, hex → -2 (non-digit character)
  {"-1",                          -2},
  {"-1\r\n",                      -2},
  {"-100",                        -2},
  {"-100\r\n",                    -2},
  {"-9223372036854775807",        -2},
  {"-0",                          -2},
  {"+5",                          -2},
  {"+0",                          -2},
  {"5abc",                        -2},
  {"abc",                         -2},
  {"5 5",                         -2},
  {"5,6",                         -2},
  {"0x10",                        -2},
  // Too large to fit in an ssize_t → -3 (overflow)
  {"99999999999999999999",        -3},
};

TEST(HeaderUtilsTest, ParseContentLengthAcceptsDigitsAndRejectsSignsAndOverflow) {
  for (const auto & [input, expected] : contentLengthCases) {
    ssize_t res = XrdHttpHeaderUtils::parseContentLength(input);
    ASSERT_EQ(expected, res) << "input was: \"" << input << "\"";
  }
}

// Each test case is (header value, expected parser return). 0 means valid
// (the list ends in "chunked"); -1 means empty; -2 means no "chunked"
// token in the list; -3 means "chunked" appears but is not the final token.
static inline const std::pair<std::string, int> transferEncodingCases[] {
  // Plain "chunked" — accepted, case-insensitive
  {"chunked",                     0},
  {"CHUNKED",                     0},
  {"Chunked",                     0},
  {"ChUnKeD",                     0},
  // Production format: parseLine forwards the trailing CRLF
  {"chunked\r\n",                 0},
  {"CHUNKED\r\n",                 0},
  // Whitespace around the token is tolerated
  {"  chunked  ",                 0},
  {"\tchunked\t",                 0},
  // Multi-token list with chunked as the LAST encoding — accepted
  {"gzip, chunked",               0},
  {"gzip, deflate, chunked",      0},
  // Chunked appears but is NOT the last encoding → -3
  {"chunked, gzip",               -3},
  {"chunked, identity",           -3},
  // The classic bugs the old strstr() match used to be fooled by → -2
  {"chunkedX",                    -2},
  {"x-chunked",                   -2},
  {"chunked-extra",               -2},
  // Lists containing only encodings we don't speak → -2
  {"gzip",                        -2},
  {"identity",                    -2},
  {"gzip, deflate",               -2},
  // Empty or whitespace-only values → -1
  {"",                            -1},
  {"\r\n",                        -1},
  {"   ",                         -1},
  {",",                           -1},
  {", ,",                         -1},
};

TEST(HeaderUtilsTest, ParseTransferEncodingRequiresChunkedAsTheLastToken) {
  for (const auto & [input, expected] : transferEncodingCases) {
    int res = XrdHttpHeaderUtils::parseTransferEncoding(input);
    ASSERT_EQ(expected, res) << "input was: \"" << input << "\"";
  }
}
