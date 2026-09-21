if(NOT TARGET XrdHttpTapeApi-${PLUGIN_VERSION})
  return()
endif()

# Compile the production adapter separately. The test supplies only the
# XrdHttpExtReq transport methods; linking XrdHttpUtils would replace that seam.
add_unit_tests(
  SOURCES
    XrdHttpTapeApi.cc
    ${PROJECT_SOURCE_DIR}/src/XrdHttpTapeApi/XrdHttpTapeApi.cc
  LIBRARIES XrdUtils
)
