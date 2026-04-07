add_executable(xrdoucutils-unit-tests XrdOucUtilsTests.cc)

target_link_libraries(xrdoucutils-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdoucutils-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
