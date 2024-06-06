add_executable(xrdoucutils-unit-tests XrdOucUtilsTests.cc)

target_link_libraries(xrdoucutils-unit-tests XrdUtils GTest::GTest GTest::Main)
target_include_directories(xrdoucutils-unit-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

gtest_discover_tests(xrdoucutils-unit-tests)
