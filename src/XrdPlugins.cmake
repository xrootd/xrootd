
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_PSS_VERSION   2.0.0 )
set( XRD_PSS_SOVERSION 2 )
set( XRD_BWM_VERSION   2.0.0 )
set( XRD_BWM_SOVERSION 2 )

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
  INTERFACE_LINK_LIBRARIES ""
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
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdPss XrdBwm
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
