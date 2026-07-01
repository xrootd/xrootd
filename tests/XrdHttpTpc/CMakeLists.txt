add_executable(xrdhttptpc-unit-tests XrdHttpTpcTests.cc)

add_library(XrdHttpTpcUtils
  ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcUtils.cc
)

target_include_directories(xrdhttptpc-unit-tests PRIVATE
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(XrdHttpTpcUtils
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto)

target_link_libraries(xrdhttptpc-unit-tests
        XrdHttpTpcUtils
        GTest::gtest
        GTest::gtest_main)

gtest_discover_tests(xrdhttptpc-unit-tests PROPERTIES DISCOVERY_TIMEOUT 10)
