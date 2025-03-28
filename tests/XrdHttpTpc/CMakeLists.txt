add_executable(xrdhttptpc-unit-tests XrdHttpTpcTests.cc)

add_library(XrdHttpTpcUtils
  ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcUtils.cc
)

target_link_libraries(xrdhttptpc-unit-tests XrdHttpTpcUtils GTest::GTest GTest::Main)

gtest_discover_tests(xrdhttptpc-unit-tests PROPERTIES DISCOVERY_TIMEOUT 10)
