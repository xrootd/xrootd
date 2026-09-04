add_executable(xrdcks-unit-tests XrdCksCombineTests.cc)

target_link_libraries(xrdcks-unit-tests
  XrdUtils
  ZLIB::ZLIB
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrdcks-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
