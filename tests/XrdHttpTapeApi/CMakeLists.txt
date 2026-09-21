if(NOT BUILD_HTTP)
  return()
endif()

# Compile the production adapter separately. The test supplies only the
# XrdHttpExtReq transport methods; linking XrdHttpUtils would replace that seam.
add_executable(xrd-http-tape-api-tests
  XrdHttpTapeApiTests.cc
  ${PROJECT_SOURCE_DIR}/src/XrdHttpTapeApi/XrdHttpTapeApi.cc)
target_link_libraries(xrd-http-tape-api-tests PRIVATE XrdUtils GTest::gtest_main)
gtest_discover_tests(xrd-http-tape-api-tests
  TEST_PREFIX XrdHttpTapeApi:: DISCOVERY_TIMEOUT 30)
