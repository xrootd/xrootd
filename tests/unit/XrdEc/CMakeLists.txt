if(NOT BUILD_XRDEC)
  return()
endif()

add_unit_tests(
  SOURCES
    XrdEc.cc
  LIBRARIES
    XrdEc
    XrdTestUtils
    ${ISAL_LIBRARIES}
  INCLUDE_DIRECTORIES
    ${ISAL_INCLUDE_DIRS}
)
