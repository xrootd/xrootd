if(NOT BUILD_MACAROONS)
  return()
endif()

# The utility functions under test are free of external dependencies, so we
# compile the source directly into the test rather than link the plugin module
# (whose symbols are hidden by a version script).

add_executable(xrdmacaroons-unit-tests
  XrdMacaroonsUtils.cc
  ${PROJECT_SOURCE_DIR}/src/XrdMacaroons/XrdMacaroonsUtils.cc
)

target_link_libraries(xrdmacaroons-unit-tests GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdmacaroons-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
