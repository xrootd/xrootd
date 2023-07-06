
add_executable(xrdcl-unit-tests
  XrdClURL.cc
  XrdClPoller.cc
  XrdClSocket.cc
  XrdClUtilsTest.cc
  ../common/Server.cc
  ../common/Utils.cc
  ../common/TestEnv.cc
)

target_link_libraries(xrdcl-unit-tests
  XrdCl
  XrdXml
  XrdUtils
  ZLIB::ZLIB
  GTest::GTest
  GTest::Main
)

target_include_directories(xrdcl-unit-tests
  PRIVATE ${CMAKE_SOURCE_DIR}/src ../common
)

gtest_discover_tests(xrdcl-unit-tests TEST_PREFIX XrdCl::)
