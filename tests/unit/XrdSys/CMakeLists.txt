add_executable(xrdsysstatx-unit-tests XrdSysStatx.cc)

target_link_libraries(xrdsysstatx-unit-tests GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdsysstatx-unit-tests TEST_PREFIX unit::XrdSys::
        PROPERTIES DISCOVERY_TIMEOUT 10)

add_executable(xrdsystimer-unit-tests XrdSysTimer.cc)

target_link_libraries(xrdsystimer-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdsystimer-unit-tests TEST_PREFIX unit::XrdSys::
        PROPERTIES DISCOVERY_TIMEOUT 10)
