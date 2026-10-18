if(NOT BUILD_CEPH)
  return()
endif()

add_executable(xrdceph-unit-tests XrdCeph.cc)

target_link_libraries(xrdceph-unit-tests
  XrdCephPosix GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdceph-unit-tests TEST_PREFIX XrdCeph::
  PROPERTIES DISCOVERY_TIMEOUT 10)
