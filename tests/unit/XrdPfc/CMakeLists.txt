# The tested code links XrdServer, which a client-only build does not have
if(NOT TARGET XrdServer)
  return()
endif()

# src/XrdPfc/XrdPfcInfo.cc and src/XrdPfc/XrdPfcDirState.cc are compiled in
# rather than linked from the plugin, as neither needs the Cache singleton.
add_unit_tests(
  SOURCES
    XrdPfcDirState.cc
    XrdPfcInfo.cc
    XrdPfcPathParseTools.cc
    ${PROJECT_SOURCE_DIR}/src/XrdPfc/XrdPfcInfo.cc
    ${PROJECT_SOURCE_DIR}/src/XrdPfc/XrdPfcDirState.cc
  LIBRARIES
    XrdServer
    XrdUtils
)
