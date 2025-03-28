if(NOT BUILD_HTTP)
    return()
endif()

add_executable(xrdhttp-unit-tests XrdHttpTests.cc)

target_link_libraries(xrdhttp-unit-tests XrdHttpUtils GTest::GTest GTest::Main)

gtest_discover_tests(xrdhttp-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
