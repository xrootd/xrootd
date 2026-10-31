if(NOT TARGET XrdHttpTapeApiStore)
  return()
endif()

add_unit_tests(
  SOURCES XrdHttpTapeApiStore.cc
  LIBRARIES XrdHttpTapeApiStore
  INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/src/XrdHttpTapeApi
)
