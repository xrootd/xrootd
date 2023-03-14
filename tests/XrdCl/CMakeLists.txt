
add_executable(xrdcl-unit-tests
  XrdClURL.cc
)

target_link_libraries(xrdcl-unit-tests
  XrdCl
  XrdXml
  XrdUtils
  GTest::GTest
  GTest::Main
)

target_include_directories(xrdcl-unit-tests
  PRIVATE ${CMAKE_SOURCE_DIR}/src
)

gtest_discover_tests(xrdcl-unit-tests TEST_PREFIX XrdCl::)
