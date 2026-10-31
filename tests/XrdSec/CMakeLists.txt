# XrdSecjwtTests.cc includes the JWT sources and needs the same libcurl and
# OpenSSL that libXrdSecjwtCore needs. Skip the suite when that plugin
# was not built (libcurl missing and FORCE_ENABLED off).
# find_package has to run in this directory: the imported CURL::libcurl
# target created for the plugin is not visible here.
if(NOT TARGET XrdSecjwtCore)
  return()
endif()

include(${CMAKE_SOURCE_DIR}/cmake/XrdCurlLink.cmake)

find_package(CURL CONFIG QUIET)
if(NOT CURL_FOUND)
  find_package(CURL MODULE REQUIRED)
endif()
find_package(OpenSSL REQUIRED)

# The test source must not be named XrdSecJWT.cc. gcov writes one short
# filename per basename, so it would mix this file with src/XrdSecjwt/XrdSecJWT.cc
# and ctest coverage would stop when the line counts disagree.
add_executable(xrdsecjwt-unit-tests
  XrdSecjwtTests.cc
)

target_link_libraries(xrdsecjwt-unit-tests
  PRIVATE XrdUtils GTest::gtest GTest::gtest_main OpenSSL::Crypto)
xrd_target_link_curl(xrdsecjwt-unit-tests)

gtest_discover_tests(xrdsecjwt-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
