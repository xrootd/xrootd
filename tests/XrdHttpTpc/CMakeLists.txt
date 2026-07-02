add_executable(xrdhttptpc-unit-tests XrdHttpTpcTests.cc)

add_library(XrdHttpTpcUtils
  ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcUtils.cc
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

if(TARGET XrdServer)
  add_executable(xrdhttptpc-stream-unit-tests
          XrdHttpTpcStreamTests.cc
          ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcStream.cc)

  target_link_libraries(xrdhttptpc-stream-unit-tests
          XrdServer
          XrdUtils
          GTest::gtest
          GTest::gtest_main)

  gtest_discover_tests(xrdhttptpc-stream-unit-tests PROPERTIES DISCOVERY_TIMEOUT 10)
endif()
