add_executable(xrdaccjwt-unit-tests
  XrdAccjwtScopeTests.cc
  ${CMAKE_SOURCE_DIR}/src/XrdAccjwt/XrdAccjwtScope.cc
)

target_include_directories(xrdaccjwt-unit-tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(xrdaccjwt-unit-tests
  XrdUtils
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrdaccjwt-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
