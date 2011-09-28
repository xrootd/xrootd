include_directories( ../ ${XROOTD_INCLUDES}  )
link_directories( ${XROOTD_LIB_DIR} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CL_VERSION   0.0.1 )
set( XRD_CL_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdCl lib
#-------------------------------------------------------------------------------
add_library(
  XrdCl
  SHARED
  XrdClLog.cc          XrdClLog.hh
  XrdClUtils.cc        XrdClUtils.hh
                       XrdClOptimizers.hh
                       XrdClConstants.hh
  XrdClEnv.cc          XrdClEnv.hh
  XrdClDefaultEnv.cc   XrdClDefaultEnv.hh
  XrdClFile.cc         XrdClFile.hh
  XrdClURL.cc          XrdClURL.hh
                       XrdClStatus.hh
  XrdClSocket.cc       XrdClSocket.hh
                       XrdClPoller.hh
)

target_link_libraries(
  XrdCl
  XrdUtils
  dl )

set_target_properties(
  XrdCl
  PROPERTIES
  VERSION   ${XRD_CL_VERSION}
  SOVERSION ${XRD_CL_SOVERSION} )

#-------------------------------------------------------------------------------
# xrdcp
#-------------------------------------------------------------------------------
add_executable(
  xrdcp
  XrdClCp.cc )

target_link_libraries(
  xrdcp
  XrdCl )

