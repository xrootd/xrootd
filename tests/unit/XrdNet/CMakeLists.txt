add_executable(xrdnet-unit-tests
  XrdNetAddrTests.cc
  XrdNetIdentityTests.cc
  XrdNetMsgTests.cc
)

target_link_libraries(xrdnet-unit-tests
  XrdUtils
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrdnet-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
