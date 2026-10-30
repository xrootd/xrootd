add_executable(xrdcks-unit-tests XrdCksCombine.cc)

target_link_libraries(xrdcks-unit-tests
  XrdUtils
  ZLIB::ZLIB
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrdcks-unit-tests TEST_PREFIX unit::XrdCks::
  PROPERTIES DISCOVERY_TIMEOUT 10)
