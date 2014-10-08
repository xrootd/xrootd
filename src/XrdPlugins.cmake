
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_BWM        XrdBwm-${PLUGIN_VERSION} )
set( LIB_XRD_PSS        XrdPss-${PLUGIN_VERSION} )
set( LIB_XRD_GPFS       XrdOssSIgpfsT-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# The XrdPss module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_PSS}
  MODULE
  XrdPss/XrdPssAio.cc
  XrdPss/XrdPss.cc           XrdPss/XrdPss.hh
  XrdPss/XrdPssCks.cc        XrdPss/XrdPssCks.hh
  XrdPss/XrdPssConfig.cc )

target_link_libraries(
  ${LIB_XRD_PSS}
  XrdFfs
  XrdPosix
  XrdUtils )

set_target_properties(
  ${LIB_XRD_PSS}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdBwm module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_BWM}
  MODULE
  XrdBwm/XrdBwm.cc             XrdBwm/XrdBwm.hh
  XrdBwm/XrdBwmConfig.cc
  XrdBwm/XrdBwmHandle.cc       XrdBwm/XrdBwmHandle.hh
  XrdBwm/XrdBwmLogger.cc       XrdBwm/XrdBwmLogger.hh
  XrdBwm/XrdBwmPolicy1.cc      XrdBwm/XrdBwmPolicy1.hh
                               XrdBwm/XrdBwmPolicy.hh
                               XrdBwm/XrdBwmTrace.hh )

target_link_libraries(
  ${LIB_XRD_BWM}
  XrdServer
  XrdUtils
  pthread )

set_target_properties(
  ${LIB_XRD_BWM}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# GPFS stat() plugin library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_GPFS}
  MODULE
  XrdOss/XrdOssSIgpfsT.cc )

target_link_libraries(
  ${LIB_XRD_GPFS}
  XrdUtils )

set_target_properties(
  ${LIB_XRD_GPFS}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_PSS} ${LIB_XRD_BWM} ${LIB_XRD_GPFS}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
