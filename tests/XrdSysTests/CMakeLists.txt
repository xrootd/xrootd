add_executable(xrdsysstatx-unit-tests XrdSysStatxTests.cc)

target_link_libraries(xrdsysstatx-unit-tests GTest::GTest GTest::Main)

gtest_discover_tests(xrdsysstatx-unit-tests
        PROPERTIES DISCOVERY_TIMEOUT 10)
