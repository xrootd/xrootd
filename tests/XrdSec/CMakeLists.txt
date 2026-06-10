find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(xrdsecoidc-unit-tests
  XrdSecOIDC.cc
)

target_link_libraries(xrdsecoidc-unit-tests
  XrdUtils GTest::GTest GTest::Main OpenSSL::Crypto CURL::libcurl)

gtest_discover_tests(xrdsecoidc-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
