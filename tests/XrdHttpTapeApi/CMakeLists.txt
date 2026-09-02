if(NOT TARGET XrdHttpTapeApiStore)
  return()
endif()

add_executable(xrd-http-tape-api-unit-tests XrdHttpTapeApiStoreTest.cc)

target_link_libraries(xrd-http-tape-api-unit-tests
  XrdHttpTapeApiStore GTest::gtest_main)

target_include_directories(xrd-http-tape-api-unit-tests
  BEFORE PRIVATE
    "${PROJECT_SOURCE_DIR}/vendor/googletest/googletest/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/XrdHttpTapeApi")

gtest_discover_tests(xrd-http-tape-api-unit-tests
  TEST_PREFIX XrdHttpTapeApi::
  PROPERTIES DISCOVERY_TIMEOUT 10)
