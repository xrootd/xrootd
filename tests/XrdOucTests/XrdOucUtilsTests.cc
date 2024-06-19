#undef NDEBUG

#include <gtest/gtest.h>
#include <string>
#include <map>

#include "XrdOuc/XrdOucUtils.hh"

#include "XrdOuc/XrdOucTUtils.hh"


using namespace testing;

class XrdOucUtilsTests : public Test {};

TEST(XrdOucUtilsTests, obfuscate) {
  // General cases
  ASSERT_EQ(std::string("scitag.flow=144&authz=") + XrdOucUtils::OBFUSCATION_STR + std::string("&test=abcd"), XrdOucUtils::obfuscate("scitag.flow=144&authz=token&test=abcd",{"authz"},'=','&'));
  ASSERT_EQ(std::string("authz=") + XrdOucUtils::OBFUSCATION_STR + std::string("&scitag.flow=144&test=abcd"), XrdOucUtils::obfuscate("authz=token&scitag.flow=144&test=abcd",{"authz"},'=','&'));
  ASSERT_EQ(std::string("scitag.flow=144&test=abcd&authz=") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscate("scitag.flow=144&test=abcd&authz=token",{"authz"},'=','&'));
  // Nothing to obfuscate
  ASSERT_EQ("test=abcd&test2=abcde",XrdOucUtils::obfuscate("test=abcd&test2=abcde",{},'=','&'));
  ASSERT_EQ("nothingtoobfuscate",XrdOucUtils::obfuscate("nothingtoobfuscate",{"obfuscateme"},'=','\n'));
  //Empty string obfuscation
  ASSERT_EQ("",XrdOucUtils::obfuscate("",{"obfuscateme"},'=','&'));
  ASSERT_EQ("",XrdOucUtils::obfuscate("",{},'=','\n'));
  // Trimmed key obfuscation
  ASSERT_EQ(std::string("Authorization:") + XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscate("Authorization: Bearer token",{"authorization"},':','\n'));
  ASSERT_EQ(std::string("Authorization:")+ XrdOucUtils::OBFUSCATION_STR, XrdOucUtils::obfuscate("Authorization : Bearer token",{"authorization"},':','\n'));
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