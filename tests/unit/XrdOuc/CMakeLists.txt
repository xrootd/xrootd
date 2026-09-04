add_executable(xrdouccrc-unit-tests XrdOucCRCTests.cc)

target_link_libraries(xrdouccrc-unit-tests
  XrdUtils
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrdouccrc-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
