add_executable(xrdouc-unit-tests XrdOucTests.cc)

target_link_libraries(xrdouc-unit-tests XrdUtils GTest::GTest GTest::Main)
target_include_directories(xrdouc-unit-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

gtest_discover_tests(xrdouc-unit-tests)
