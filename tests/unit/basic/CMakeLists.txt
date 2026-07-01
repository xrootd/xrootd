add_executable(basic-tests-unit mock.cc
        )

target_link_libraries(basic-tests-unit GTest::gmock GTest::gtest GTest::gtest_main)

gtest_discover_tests(basic-tests-unit
  PROPERTIES DISCOVERY_TIMEOUT 10)
