add_executable(xrdmetrics-unit-tests
  XrdMetricsTests.cc
  XrdMetricsRegistryTests.cc)

target_link_libraries(xrdmetrics-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdmetrics-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
