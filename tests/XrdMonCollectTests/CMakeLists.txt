add_executable(xrdmoncollect-unit-tests XrdMonCollectTests.cc)

target_link_libraries(xrdmoncollect-unit-tests
  XrdAppUtils
  XrdUtils
  GTest::gtest
  GTest::gtest_main)

gtest_discover_tests(xrdmoncollect-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
