add_executable(xrdtpc-unit-tests XrdTpcTests.cc)

add_library(XrdTpcUtils
        ${CMAKE_SOURCE_DIR}/src/XrdTpc/XrdTpcUtils.cc)

target_link_libraries(xrdtpc-unit-tests XrdTpcUtils GTest::GTest GTest::Main)
target_include_directories(xrdtpc-unit-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

gtest_discover_tests(xrdtpc-unit-tests)
