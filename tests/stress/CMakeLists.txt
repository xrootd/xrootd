
if( NOT CURL_FOUND )
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

