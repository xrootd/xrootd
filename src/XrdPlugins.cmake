
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_PSS_VERSION   0.0.1 )
set( XRD_PSS_SOVERSION 0 )
set( XRD_BWM_VERSION   0.0.1 )
set( XRD_BWM_SOVERSION 0 )

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
  XrdFfs )

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
  XrdServer )

set_target_properties(
  XrdBwm
  PROPERTIES
  VERSION   ${XRD_BWM_VERSION}
  SOVERSION ${XRD_BWM_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdPss XrdBwm
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdPss
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdPss
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  DIRECTORY      XrdBwm/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdBwm
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
