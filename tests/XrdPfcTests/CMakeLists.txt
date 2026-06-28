add_executable(xrdpfc-unit-tests XrdPfcTests.cc)

target_link_libraries(xrdpfc-unit-tests GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdpfc-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)

# Cache metrics unit test. XrdPfc is a loadable module, so the self-contained
# CacheMetrics source is compiled directly into the test; it only needs the
# header-only registry/instruments and the serializer from XrdUtils.
add_executable(xrdpfc-metrics-tests
  XrdPfcMetricsTests.cc
  ${CMAKE_SOURCE_DIR}/src/XrdPfc/XrdPfcMetrics.cc)

target_link_libraries(xrdpfc-metrics-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdpfc-metrics-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
