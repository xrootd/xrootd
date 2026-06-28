add_executable(xrdmetrics-unit-tests
  XrdMetricsRegistryTests.cc
  XrdHttpMetricsExporterConfigTests.cc)

target_link_libraries(xrdmetrics-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdmetrics-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)

# End-to-end transport test for the metrics exporter push helper; needs libcurl,
# matching the plugin's own optional push support.
find_package(CURL)
if(CURL_FOUND)
  add_executable(xrdmetrics-push-tests
    XrdHttpMetricsExporterPushTests.cc)
  target_compile_definitions(xrdmetrics-push-tests PRIVATE XRDMETRICS_HAVE_CURL)
  target_link_libraries(xrdmetrics-push-tests
    XrdUtils CURL::libcurl GTest::gtest GTest::gtest_main)
  gtest_discover_tests(xrdmetrics-push-tests
    PROPERTIES DISCOVERY_TIMEOUT 10)
endif()
