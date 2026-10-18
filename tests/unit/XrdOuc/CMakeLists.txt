add_executable(xrdouccrc-unit-tests XrdOucCRCTests.cc)

target_link_libraries(xrdouccrc-unit-tests
  XrdUtils
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrdouccrc-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)

add_executable(xrdouc-hashval-unit-tests XrdOucHashValTests.cc)

target_link_libraries(xrdouc-hashval-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdouc-hashval-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)

add_executable(xrdoucutils-unit-tests XrdOucUtilsTests.cc)

target_link_libraries(xrdoucutils-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdoucutils-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
