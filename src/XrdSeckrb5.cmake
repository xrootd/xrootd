include_directories( ${KERBEROS5_INCLUDE_DIR} )
include( XRootDCommon )

#-------------------------------------------------------------------------------
# The XrdSeckrb5 module
#-------------------------------------------------------------------------------
set( LIB_XRD_SEC_KRB5 XrdSeckrb5-${PLUGIN_VERSION} )

add_library(
  ${LIB_XRD_SEC_KRB5}
  MODULE
  XrdSeckrb5/XrdSecProtocolkrb5.cc )

target_link_libraries(
  ${LIB_XRD_SEC_KRB5}
  XrdUtils
  ${KERBEROS5_LIBRARIES} )

set_target_properties(
  ${LIB_XRD_SEC_KRB5}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_SEC_KRB5}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
