add_executable(xrdsysstatx-unit-tests XrdSysStatxTests.cc)

target_link_libraries(xrdsysstatx-unit-tests GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdsysstatx-unit-tests
        PROPERTIES DISCOVERY_TIMEOUT 10)

add_executable(xrdsystimer-unit-tests XrdSysTimerTests.cc)

target_link_libraries(xrdsystimer-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdsystimer-unit-tests
        PROPERTIES DISCOVERY_TIMEOUT 10)
