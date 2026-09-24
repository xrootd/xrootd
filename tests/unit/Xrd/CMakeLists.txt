add_executable(xrdscheduler-unit-tests XrdSchedulerTests.cc)

target_link_libraries(xrdscheduler-unit-tests
  XrdUtils
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrdscheduler-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
