#undef NDEBUG

#include <gtest/gtest.h>
#include <map>
#include <string>

#include "XrdOuc/XrdOucTUtils.hh"


using namespace testing;

class XrdOucTests : public Test {};

TEST(XrdOucTests, caseInsensitiveFindTest) {
  std::map<std::string,int> map;
  map["Test"] = 1;
  map["abcd"] = 2;
  map["ABCD"] = 3;
  auto it = XrdOucTUtils::caseInsensitiveFind(map,"test");

  ASSERT_NE(map.end(), it);
  ASSERT_EQ(1, it->second);

  it = XrdOucTUtils::caseInsensitiveFind(map,"does_not_exist");
  ASSERT_EQ(map.end(),it);

  std::map<std::string, int> emptyMap;
  ASSERT_EQ(map.end(), XrdOucTUtils::caseInsensitiveFind(map,"does_not_exist"));
}