add_executable(xrdec-unit-tests
  MicroTest.cc
  ../common/Server.cc
  ../common/Utils.cc
  ../common/TestEnv.cc
)

target_link_libraries(xrdec-unit-tests
  XrdEc
  XrdCl
  XrdXml
  XrdUtils
  ZLIB::ZLIB
  GTest::GTest
  GTest::Main
  ${ISAL_LIBRARIES}
)

target_include_directories(xrdec-unit-tests
  PRIVATE ${CMAKE_SOURCE_DIR}/src
  PRIVATE ../common
  ${ISAL_INCLUDE_DIRS}
)

gtest_discover_tests(xrdec-unit-tests TEST_PREFIX XrdCl::)
