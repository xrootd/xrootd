#include_directories( ${KERBEROS5_INCLUDE_DIR} )
include( XRootDCommon )

#-------------------------------------------------------------------------------
# The XrdSecztn module
#-------------------------------------------------------------------------------
set( LIB_XRD_SEC_ZTN XrdSecztn-${PLUGIN_VERSION} )

add_library(
  ${LIB_XRD_SEC_ZTN}
  MODULE
  XrdSecztn/XrdSecProtocolztn.cc
  XrdSecztn/XrdSecztn.cc )

target_link_libraries(
  ${LIB_XRD_SEC_ZTN}
  XrdUtils
  )

set_target_properties(
  ${LIB_XRD_SEC_ZTN}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_SEC_ZTN}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
