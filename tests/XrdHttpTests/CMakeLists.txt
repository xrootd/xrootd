if(NOT BUILD_HTTP)
    return()
endif()

add_executable(xrdhttp-unit-tests XrdHttpTests.cc)

target_link_libraries(xrdhttp-unit-tests XrdHttpUtils GTest::GTest GTest::Main)
target_include_directories(xrdhttp-unit-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

gtest_discover_tests(xrdhttp-unit-tests)
