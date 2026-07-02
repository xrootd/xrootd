add_executable(example-tests-unit 
        example_gmock.cc
        )

target_link_libraries(example-tests-unit 
        GTest::gmock 
        GTest::gtest 
        GTest::gtest_main
        )

gtest_discover_tests(example-tests-unit
        PROPERTIES DISCOVERY_TIMEOUT 10)
