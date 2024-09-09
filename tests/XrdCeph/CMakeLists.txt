if(NOT BUILD_CEPH)
  return()
endif()

add_executable(xrdceph-unit-tests XrdCeph.cc)

target_link_libraries(xrdceph-unit-tests
  XrdCephPosix GTest::GTest GTest::Main)

gtest_discover_tests(xrdceph-unit-tests TEST_PREFIX XrdCeph::)
