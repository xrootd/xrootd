# Curl is necessary for this test
find_package(CURL)

# If XRootD::http fixture is not available, we
# cannot run the HTTP stress test.
if(NOT CURL_FOUND OR NOT ENABLE_HTTP_TESTS)
  return()
endif()

include(GoogleTest)

add_executable( xrdhttp-stress-tests
  curl_stress.cc
)

target_link_libraries( xrdhttp-stress-tests
  GTest::GTest GTest::Main CURL::libcurl
)

gtest_add_tests( TARGET xrdhttp-stress-tests
  TEST_LIST xrdHttpStressTests
)

set_tests_properties( ${xrdHttpStressTests}
  PROPERTIES
    FIXTURES_REQUIRED XRootD::http
  ENVIRONMENT "TEST_CONFIG=${CMAKE_BINARY_DIR}/tests/XRootD/http/data/test_config.sh"
)

