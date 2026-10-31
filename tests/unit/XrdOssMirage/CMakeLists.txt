# The tested code links XrdServer, which a client-only build does not have
if(NOT TARGET XrdServer)
  return()
endif()

add_unit_tests(
  SOURCES
    XrdOssMirage.cc
    XrdOssMirageFile.cc
    XrdOssMirageXAttr.cc
    ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirage.cc
    ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageDir.cc
    ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageFile.cc
    ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageXAttr.cc
  LIBRARIES
    XrdServer
    XrdUtils
)
