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
