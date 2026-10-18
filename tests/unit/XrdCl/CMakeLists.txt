add_executable(xrdcl-tests-unit
  XrdClPlugInManagerTests.cc
  XrdClTPFallBackCopyJobTests.cc
)

target_link_libraries(xrdcl-tests-unit GTest::gmock GTest::gtest GTest::gtest_main XrdCl)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdcl-tests-unit stdc++fs)
endif()

# The tests load the recorder plug-in from their config files
add_dependencies(xrdcl-tests-unit XrdClRecorder-${PLUGIN_VERSION})

gtest_discover_tests(xrdcl-tests-unit
  PROPERTIES DISCOVERY_TIMEOUT 10)

add_executable(xrdcl-unit-tests
  XrdClEnv.cc
  XrdClURL.cc
  XrdClUtilsTest.cc
  XrdClProtocolRespTest.cc
  XrdClLocalFileHandlerTest.cc
  )

target_link_libraries(xrdcl-unit-tests
  XrdTestUtils GTest::gtest GTest::gtest_main)

# LocalFileHandlerTest uses std::filesystem
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdcl-unit-tests stdc++fs)
endif()

gtest_discover_tests(xrdcl-unit-tests TEST_PREFIX XrdCl:: 
  PROPERTIES DISCOVERY_TIMEOUT 10)
