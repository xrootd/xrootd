# The tested code links XrdHttpUtils, which a client-only build does not have
if(NOT BUILD_HTTP OR NOT TARGET XrdHttpUtils)
  return()
endif()

add_unit_tests(
  SOURCES
    XrdHttpChecksumHandler.cc
    XrdHttpHeaderUtils.cc
    XrdHttpReadRangeHandler.cc
    XrdHttpUtils.cc
  LIBRARIES
    XrdHttpUtils
    XrdUtils
)
