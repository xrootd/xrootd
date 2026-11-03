#undef NDEBUG

#include "XrdHttpCors/XrdHttpCorsHandler.hh"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <unordered_set>

using namespace testing;

TEST(CorsHandlerTest, GetCorsAllowOriginHeaderReturnsHeaderOnlyForAllowedOrigins) {
  std::unordered_set<std::string> allowedOrigins = {
    "https://helloworld.cern.ch",
    "https://anotherorigins.cern.ch"
  };
  XrdHttpCorsHandler corsHandler;
  for (const auto &allowedOrigin: allowedOrigins) {
    corsHandler.addAllowedOrigin(allowedOrigin);
  }
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader("test"));
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader(""));
  for (const auto &allowedOrigin: allowedOrigins) {
    std::string expected{"Access-Control-Allow-Origin: " + allowedOrigin};
    ASSERT_EQ(expected, corsHandler.getCORSAllowOriginHeader(allowedOrigin));
  }
  corsHandler.addAllowedOrigin("");
  corsHandler.addAllowedOrigin(" ");
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader(""));
  ASSERT_EQ(std::nullopt, corsHandler.getCORSAllowOriginHeader(" "));
}
