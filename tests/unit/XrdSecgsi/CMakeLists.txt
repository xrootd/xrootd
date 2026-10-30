add_executable(xrdsecgsi-tests-unit rtag.cc)

target_link_libraries(xrdsecgsi-tests-unit
        XrdCrypto
        XrdUtils
        GTest::gtest
        GTest::gtest_main
        )

gtest_discover_tests(xrdsecgsi-tests-unit TEST_PREFIX unit::XrdSecgsi::
        PROPERTIES DISCOVERY_TIMEOUT 10)
