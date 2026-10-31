if(NOT BUILD_MACAROONS)
  return()
endif()

# The utility functions under test are free of external dependencies, so we
# compile the source directly into the test rather than link the plugin module
# (whose symbols are hidden by a version script).

add_unit_tests(
  SOURCES
    XrdMacaroonsUtils.cc
    ${PROJECT_SOURCE_DIR}/src/XrdMacaroons/XrdMacaroonsUtils.cc
)
