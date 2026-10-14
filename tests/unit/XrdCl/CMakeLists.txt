add_executable(xrdcl-tests-unit XrdClTPFallBackCopyJobTests.cc
        )

target_link_libraries(xrdcl-tests-unit GTest::gmock GTest::gtest GTest::gtest_main XrdCl)

gtest_discover_tests(xrdcl-tests-unit
  PROPERTIES DISCOVERY_TIMEOUT 10)
