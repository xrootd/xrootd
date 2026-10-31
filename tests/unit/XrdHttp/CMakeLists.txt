# The tested code links XrdHttpUtils, which a client-only build does not have
if(NOT BUILD_HTTP OR NOT TARGET XrdHttpUtils)
    return()
endif()

add_executable(xrdhttp-unit-tests XrdHttp.cc
        ${PROJECT_SOURCE_DIR}/src/XrdHttpCors/XrdHttpCorsHandler.cc)

target_link_libraries(xrdhttp-unit-tests XrdHttpUtils GTest::gtest GTest::gtest_main XrdUtils)

gtest_discover_tests(xrdhttp-unit-tests TEST_PREFIX unit::XrdHttp::
  PROPERTIES DISCOVERY_TIMEOUT 10)
