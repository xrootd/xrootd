#undef NDEBUG

#include "XrdHttpTpc/XrdHttpTpcUtils.hh"
#include "XrdHttpTpc/XrdHttpTpcTPC.hh"
#include <exception>
#include <gtest/gtest.h>
#include <string>

using namespace testing;

class XrdHttpTpcTests : public Test {};

TEST(XrdHttpTpcTests, prepareOpenURLTest) {
  std::string resource = "/eos/test/file.txt";
  {
    // Nothing to set in the openURL
    std::map<std::string, std::string> headers {{"Test","Test"}};
    std::map<std::string, std::string> hdr2cgi {};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(resource,headers,hdr2cgi);

    ASSERT_EQ(resource + "?" + TPC::TPCHandler::OSS_TASK_OPAQUE.data(),openURL);
  }

  {
    // If authz= was put in the opaque of the resource (and therefore put in the xrd-http-query header),
    // Then the authorization header should have been set and no "authz" should be found in the opaque of
    // the open URL
    std::map<std::string, std::string> headers {{"xrd-http-query","authz=test&scitag.flow=144"}};
    std::map<std::string, std::string> hdr2cgi {};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(resource,headers,hdr2cgi);

    ASSERT_NE(resource,openURL);
    ASSERT_TRUE(headers.find("Authorization") != headers.end());
    ASSERT_TRUE(openURL.find("&authz") == std::string::npos);
    ASSERT_TRUE(openURL.find("&scitag.flow") != std::string::npos);
    ASSERT_TRUE(openURL.find(std::string("?") + TPC::TPCHandler::OSS_TASK_OPAQUE.data()) != std::string::npos);
  }

  {
    // If authz= was put in the opaque of the resource (and therefore put in the xrd-http-query header),
    // and if the the authorization header is provided, we should not override the provided authorization header
    std::map<std::string, std::string> headers {{"xrd-http-query","authz=test&scitag.flow=144"},{"Authorization","abcd"}};
    std::map<std::string, std::string> hdr2cgi {};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(resource,headers,hdr2cgi);

    ASSERT_NE(resource,openURL);
    ASSERT_TRUE(openURL.find("&authz") == std::string::npos);
    ASSERT_TRUE(openURL.find("&scitag.flow") != std::string::npos);
    ASSERT_EQ("abcd",headers["Authorization"]);
  }

  {
    // Some hdr2cgi has been configured, we should find them in the opaque of the openURL
    std::map<std::string, std::string> headers {{"xrd-http-query","authz=test&test1=test2"},{"Scitag","144"},{"lowercase_header","test1"} };
    std::map<std::string, std::string> hdr2cgi {{"SciTag","scitag.flow"},{"LOWERCASE_HEADER","lowercase"}};
    auto openURL = XrdHttpTpcUtils::prepareOpenURL(resource,headers,hdr2cgi);

    ASSERT_TRUE(openURL.find("&test1=test2") != std::string::npos);
    ASSERT_TRUE(openURL.find("&scitag.flow=144") != std::string::npos);
    ASSERT_TRUE(openURL.find("&lowercase=test1") != std::string::npos);
    ASSERT_TRUE(openURL.find(std::string("?") + TPC::TPCHandler::OSS_TASK_OPAQUE.data()) != std::string::npos);
  }

}
