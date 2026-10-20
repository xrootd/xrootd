if(NOT TARGET XrdServer)
  return()
endif()

add_executable(xrdoss-cache-unit-tests XrdOssCacheTests.cc)
target_link_libraries(xrdoss-cache-unit-tests
  XrdServer XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdoss-cache-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
