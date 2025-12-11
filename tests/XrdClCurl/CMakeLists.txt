# XrdClCurl tests depend on scitokens fixture for authentication
if(NOT BUILD_SCITOKENS)
  return()
endif()

# The final shared library has its symbols stripped; we need the symbols available
# to allow unit tests to access them.  Rather than build the objects twice, we
# create an internal object library and just link twice -- once stripping the symbols,
# once without.
add_library(XrdClCurlTesting SHARED)
target_link_libraries(XrdClCurlTesting PRIVATE XrdClCurlObj)

include(GoogleTest)

find_program(OPENSSL_BIN openssl REQUIRED)

# All tests that only depend on the pure XrdCl interfaces and
# have no linkage against XrdClCurl
add_executable(xrdcl-test
  ChecksumTest.cc
  DeleteTest.cc
  MkcolTest.cc
  ReadTest.cc
  WriteTest.cc
)

# Tests depending on XrdClCurl linkage
add_executable(xrdcl-curl-test
  CopyTest.cc
  ParseTimeoutTest.cc
  VectorReadTest.cc
)

target_link_libraries(xrdcl-test XrdClCurlTransferTest GTest::gtest_main)
target_link_libraries(xrdcl-curl-test XrdClCurlTesting XrdClCurlTransferTest GTest::gtest_main)

gtest_add_tests(TARGET xrdcl-test TEST_LIST CurlClOnlyTests)
gtest_add_tests(TARGET xrdcl-curl-test TEST_LIST CurlTests)

set_tests_properties(${CurlTests} ${CurlClOnlyTests}
  PROPERTIES
    FIXTURES_REQUIRED
      XrdClCurl::curl
    ENVIRONMENT
      "ENV_FILE=${CMAKE_BINARY_DIR}/tests/curl/setup.sh;XRD_PLUGINCONFDIR=${CMAKE_BINARY_DIR}/tests/curl/client.plugins.d"
)

######################################
# Integration tests.
######################################
add_test(NAME XrdClCurl::curl::setup
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/curl-setup.sh" curl)

set_tests_properties(XrdClCurl::curl::setup
  PROPERTIES
    FIXTURES_SETUP
      XrdClCurl::curl
    ENVIRONMENT
      "OPENSSL_BIN=${OPENSSL_BIN};BINARY_DIR=${CMAKE_BINARY_DIR};SOURCE_DIR=${CMAKE_SOURCE_DIR}"
)

add_test(NAME XrdClCurl::curl::teardown
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/curl-teardown.sh" curl)

set_tests_properties(XrdClCurl::curl::teardown
  PROPERTIES
    FIXTURES_CLEANUP
      XrdClCurl::curl
    ENVIRONMENT
      "BINARY_DIR=${CMAKE_BINARY_DIR}"
)

add_test(NAME XrdClCurl::curl::test
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/curl-test.sh" curl)

list(APPEND CURL_TEST_LOGS ${CMAKE_CURRENT_BINARY_DIR}/tests/curl/origin.log)
list(APPEND CURL_TEST_LOGS ${CMAKE_CURRENT_BINARY_DIR}/tests/curl/cache.log)
list(APPEND CURL_TEST_LOGS ${CMAKE_CURRENT_BINARY_DIR}/tests/curl/client.log)

set_tests_properties(XrdClCurl::curl::test
  PROPERTIES
    FIXTURES_REQUIRED
      XrdClCurl::curl
    ENVIRONMENT
      "BINARY_DIR=${CMAKE_BINARY_DIR}"
    ATTACHED_FILES_ON_FAIL
      "${CURL_TEST_LOGS}"
)
