# The tested code is part of the server, which a client-only build does not have
if(NOT TARGET XrdServer)
  return()
endif()

add_unit_tests(
  SOURCES
    XrdHttpCorsHandler.cc
    ${PROJECT_SOURCE_DIR}/src/XrdHttpCors/XrdHttpCorsHandler.cc
  LIBRARIES
    XrdUtils
)
