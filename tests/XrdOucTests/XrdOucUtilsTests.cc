#undef NDEBUG

#include <gtest/gtest.h>
#include <string>
#include <map>

#include "XrdOuc/XrdOucUtils.hh"

#include "XrdOuc/XrdOucTUtils.hh"
#include "XrdOuc/XrdOucPrivateUtils.hh"


using namespace testing;

// duplicated here to avoid becoming a public symbol of XrdUtils
static const std::string OBFUSCATION_STR = "REDACTED";

class XrdOucUtilsTests : public Test {};

TEST(XrdOucUtilsTests, obfuscateAuth) {
  // General cases
  ASSERT_EQ(std::string("scitag.flow=144&authz=") + OBFUSCATION_STR + std::string("&test=abcd"), obfuscateAuth("scitag.flow=144&authz=token&test=abcd"));
  ASSERT_EQ(std::string("authz=") + OBFUSCATION_STR + std::string("&scitag.flow=144&test=abcd"), obfuscateAuth("authz=token&scitag.flow=144&test=abcd"));
  ASSERT_EQ(std::string("scitag.flow=144&test=abcd&authz=") + OBFUSCATION_STR, obfuscateAuth("scitag.flow=144&test=abcd&authz=token"));
  // Nothing to obfuscate
  ASSERT_EQ("test=abcd&test2=abcde",obfuscateAuth("test=abcd&test2=abcde"));
  ASSERT_EQ("nothingtoobfuscate",obfuscateAuth("nothingtoobfuscate"));
  //Empty string obfuscation
  ASSERT_EQ("",obfuscateAuth(""));
  //2 authz to obfuscate
  ASSERT_EQ(std::string("authz=") + OBFUSCATION_STR + std::string("&test=test2&authz=") + OBFUSCATION_STR,obfuscateAuth("authz=abcd&test=test2&authz=abcdef"));
  // Trimmed key obfuscation
  ASSERT_EQ(std::string("Authorization: ") + OBFUSCATION_STR, obfuscateAuth("Authorization: Bearer token"));
  ASSERT_EQ(std::string("Authorization :") + OBFUSCATION_STR, obfuscateAuth("Authorization :Bearer token"));
  ASSERT_EQ(std::string("authorization :") + OBFUSCATION_STR, obfuscateAuth("authorization :Bearer token"));
  ASSERT_EQ(std::string("transferHeaderauthorization :") + OBFUSCATION_STR, obfuscateAuth("transferHeaderauthorization :Bearer token"));
  // Different obfuscation
  ASSERT_EQ(std::string("(message: kXR_stat (path: /tmp/xrootd/public/foo?authz=") + OBFUSCATION_STR + std::string("&pelican.timeout=3s, flags: none) )."), obfuscateAuth("(message: kXR_stat (path: /tmp/xrootd/public/foo?authz=foo1234&pelican.timeout=3s, flags: none) )."));
  ASSERT_EQ(std::string("(message: kXR_stat (path: /tmp/xrootd/public/foo?pelican.timeout=3s&authz=") + OBFUSCATION_STR + std::string(", flags: none) )."), obfuscateAuth("(message: kXR_stat (path: /tmp/xrootd/public/foo?pelican.timeout=3s&authz=foo1234, flags: none) )."));
  ASSERT_EQ(std::string("/path/test.txt?scitag.flow=44&authz=") + OBFUSCATION_STR + std::string(" done close."),obfuscateAuth("/path/test.txt?scitag.flow=44&authz=abcdef done close."));
  ASSERT_EQ(std::string("Appended header field to opaque info: 'authz=") + OBFUSCATION_STR, obfuscateAuth("Appended header field to opaque info: 'authz=Bearer abcdef'"));
  ASSERT_EQ(std::string("Appended header fields to opaque info: 'authz=") + OBFUSCATION_STR + std::string("&scitag.flow=65'"), obfuscateAuth("Appended header fields to opaque info: 'authz=Bearer token&scitag.flow=65'"));
  ASSERT_EQ(std::string("Processing source entry: /etc/passwd, type local file, target file: root://localhost:1094//tmp/passwd?authz=") + OBFUSCATION_STR, obfuscateAuth("Processing source entry: /etc/passwd, type local file, target file: root://localhost:1094//tmp/passwd?authz=testabcd"));
}

TEST(XrdOucUtilsTests, caseInsensitiveFind) {
  {
    std::map<std::string, std::string> map;
    ASSERT_EQ(map.end(), XrdOucTUtils::caseInsensitiveFind(map, "test"));
  }
  {
    std::map<std::string, std::string> map { {"test","lowercase"}, {"TEST2","uppercase"},{"AnotherTest", "UpperCamelCase"}};

    ASSERT_EQ("lowercase", XrdOucTUtils::caseInsensitiveFind(map, "test")->second);
    ASSERT_EQ("uppercase", XrdOucTUtils::caseInsensitiveFind(map, "test2")->second);
    ASSERT_EQ("UpperCamelCase", XrdOucTUtils::caseInsensitiveFind(map, "anothertest")->second);
    map[""] = "empty";
    ASSERT_EQ("empty", XrdOucTUtils::caseInsensitiveFind(map, "")->second);
  }
}
