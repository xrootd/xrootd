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

add_executable(xrdcl-timeout-tests-unit ClientTimeoutTests.cc)
target_link_libraries(xrdcl-timeout-tests-unit
  GTest::gtest GTest::gtest_main XrdCl XrdUtils)
gtest_discover_tests(xrdcl-timeout-tests-unit
  PROPERTIES DISCOVERY_TIMEOUT 10)
