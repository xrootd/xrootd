#undef NDEBUG

#include <gtest/gtest.h>
#include <string>
#include <map>

#include "XrdOuc/XrdOucUtils.hh"

#include "XrdOuc/XrdOucTUtils.hh"


using namespace testing;

class XrdOucUtilsTests : public Test {};

TEST(XrdOucUtilsTests, obfuscateAuth) {
  // General cases
  ASSERT_EQ(std::string("scitag.flow=144&authz=") + XrdOucUtils::OBFUSCATION_STR + std::string("&test=abcd"), XrdOucUtils::obfuscateAuth("scitag.flow=144&authz=token&test=abcd"));
  ASSERT_EQ(std::string("authz=") + XrdOucUtils::OBFUSCATION_STR + std::string("&scitag.flow=144&test=abcd"), XrdOucUtils::obfuscateAuth("authz=token&scitag.flow=144&test=abcd"));
  ASSERT_EQ(std::string("scitag.flow=144&test=abcd&authz=") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscateAuth("scitag.flow=144&test=abcd&authz=token"));
  // Nothing to obfuscate
  ASSERT_EQ("test=abcd&test2=abcde",XrdOucUtils::obfuscateAuth("test=abcd&test2=abcde"));
  ASSERT_EQ("nothingtoobfuscate",XrdOucUtils::obfuscateAuth("nothingtoobfuscate"));
  //Empty string obfuscation
  ASSERT_EQ("",XrdOucUtils::obfuscateAuth(""));
  //2 authz to obfuscate
  ASSERT_EQ(std::string("authz=") + XrdOucUtils::OBFUSCATION_STR + std::string("&test=test2&authz=") + XrdOucUtils::OBFUSCATION_STR,XrdOucUtils::obfuscateAuth("authz=abcd&test=test2&authz=abcdef"));
  // Trimmed key obfuscation
  ASSERT_EQ(std::string("Authorization: ") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscateAuth("Authorization: Bearer token"));
  ASSERT_EQ(std::string("Authorization :") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscateAuth("Authorization :Bearer token"));
  ASSERT_EQ(std::string("authorization :") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscateAuth("authorization :Bearer token"));
  ASSERT_EQ(std::string("transferHeaderauthorization :") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscateAuth("transferHeaderauthorization :Bearer token"));
  // Different obfuscation
  ASSERT_EQ(std::string("(message: kXR_stat (path: /tmp/xrootd/public/foo?authz=") + XrdOucUtils::OBFUSCATION_STR + std::string("&pelican.timeout=3s, flags: none) )."), XrdOucUtils::obfuscateAuth("(message: kXR_stat (path: /tmp/xrootd/public/foo?authz=foo1234&pelican.timeout=3s, flags: none) )."));
  ASSERT_EQ(std::string("(message: kXR_stat (path: /tmp/xrootd/public/foo?pelican.timeout=3s&authz=") + XrdOucUtils::OBFUSCATION_STR + std::string(", flags: none) )."), XrdOucUtils::obfuscateAuth("(message: kXR_stat (path: /tmp/xrootd/public/foo?pelican.timeout=3s&authz=foo1234, flags: none) )."));
  ASSERT_EQ(std::string("/path/test.txt?scitag.flow=44&authz=") + XrdOucUtils::OBFUSCATION_STR + std::string(" done close."),XrdOucUtils::obfuscateAuth("/path/test.txt?scitag.flow=44&authz=abcdef done close."));
  ASSERT_EQ(std::string("Appended header field to opaque info: 'authz=") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscateAuth("Appended header field to opaque info: 'authz=Bearer abcdef'"));
  ASSERT_EQ(std::string("Appended header fields to opaque info: 'authz=") + XrdOucUtils::OBFUSCATION_STR + std::string("&scitag.flow=65'"), XrdOucUtils::obfuscateAuth("Appended header fields to opaque info: 'authz=Bearer token&scitag.flow=65'"));
  ASSERT_EQ(std::string("Processing source entry: /etc/passwd, type local file, target file: root://localhost:1094//tmp/passwd?authz=") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscateAuth("Processing source entry: /etc/passwd, type local file, target file: root://localhost:1094//tmp/passwd?authz=testabcd"));
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