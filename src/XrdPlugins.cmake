
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_PSS_VERSION   1.0.0 )
set( XRD_PSS_SOVERSION 1 )
set( XRD_BWM_VERSION   1.0.0 )
set( XRD_BWM_SOVERSION 1 )
set( XRD_THROTTLE_VERSION 1.0.0 )
set( XRD_THROTTLE_SOVERSION 1 )

#-------------------------------------------------------------------------------
# The XrdPss lib
#-------------------------------------------------------------------------------
add_library(
  XrdPss
  SHARED
  XrdPss/XrdPssAio.cc
  XrdPss/XrdPss.cc           XrdPss/XrdPss.hh
  XrdPss/XrdPssCks.cc        XrdPss/XrdPssCks.hh
  XrdPss/XrdPssConfig.cc )

target_link_libraries(
  XrdPss
  XrdFfs
  XrdPosix
  XrdUtils )

set_target_properties(
  XrdPss
  PROPERTIES
  VERSION   ${XRD_PSS_VERSION}
  SOVERSION ${XRD_PSS_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdBwm lib
#-------------------------------------------------------------------------------
add_library(
  XrdBwm
  SHARED
  XrdBwm/XrdBwm.cc             XrdBwm/XrdBwm.hh
  XrdBwm/XrdBwmConfig.cc
  XrdBwm/XrdBwmHandle.cc       XrdBwm/XrdBwmHandle.hh
  XrdBwm/XrdBwmLogger.cc       XrdBwm/XrdBwmLogger.hh
  XrdBwm/XrdBwmPolicy1.cc      XrdBwm/XrdBwmPolicy1.hh
                               XrdBwm/XrdBwmPolicy.hh
                               XrdBwm/XrdBwmTrace.hh )

target_link_libraries(
  XrdBwm
  XrdServer
  XrdUtils
  pthread )

set_target_properties(
  XrdBwm
  PROPERTIES
  VERSION   ${XRD_BWM_VERSION}
  SOVERSION ${XRD_BWM_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdThrottle lib
#-------------------------------------------------------------------------------
add_library(
  XrdThrottle
  SHARED
  XrdOfs/XrdOfsFS.cc
  XrdThrottle/XrdThrottle.hh           XrdThrottle/XrdThrottleTrace.hh
  XrdThrottle/XrdThrottleFileSystem.cc
  XrdThrottle/XrdThrottleFileSystemConfig.cc
  XrdThrottle/XrdThrottleFile.cc
  XrdThrottle/XrdThrottleManager.cc    XrdThrottle/XrdThrottleManager.hh
)

target_link_libraries(
  XrdThrottle
  XrdUtils )

set_target_properties(
  XrdThrottle
  PROPERTIES
  VERSION    ${XRD_THROTTLE_VERSION}
  SOVERSION  ${XRD_THROTTLE_VERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdPss XrdBwm XrdThrottle
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
