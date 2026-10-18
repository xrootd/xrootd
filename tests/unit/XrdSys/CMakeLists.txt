add_executable(xrdsysstatx-unit-tests XrdSysStatxTests.cc)

target_link_libraries(xrdsysstatx-unit-tests GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdsysstatx-unit-tests TEST_PREFIX unit::XrdSys::
        PROPERTIES DISCOVERY_TIMEOUT 10)