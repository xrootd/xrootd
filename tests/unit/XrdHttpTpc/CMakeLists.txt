add_executable(xrdhttptpc-unit-tests XrdHttpTpc.cc)

add_library(XrdHttpTpcUtils
  ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcUtils.cc
)

target_link_libraries(XrdHttpTpcUtils
        PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto)

target_link_libraries(xrdhttptpc-unit-tests
        XrdHttpTpcUtils
        GTest::gtest
        GTest::gtest_main)

gtest_discover_tests(xrdhttptpc-unit-tests TEST_PREFIX unit::XrdHttpTpc:: PROPERTIES DISCOVERY_TIMEOUT 10)

if(TARGET XrdServer)
  add_executable(xrdhttptpc-stream-unit-tests
          XrdHttpTpcStream.cc
          ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcStream.cc)

  target_link_libraries(xrdhttptpc-stream-unit-tests
          XrdServer
          XrdUtils
          GTest::gtest
          GTest::gtest_main)

  gtest_discover_tests(xrdhttptpc-stream-unit-tests TEST_PREFIX unit::XrdHttpTpc:: PROPERTIES DISCOVERY_TIMEOUT 10)
endif()
