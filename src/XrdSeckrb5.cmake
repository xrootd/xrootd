include_directories( ${KERBEROS5_INCLUDE_DIR} )
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SEC_KRB5_VERSION   2.0.0 )
set( XRD_SEC_KRB5_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdSeckrb5 library
#-------------------------------------------------------------------------------
add_library(
  XrdSeckrb5
  SHARED
  XrdSeckrb5/XrdSecProtocolkrb5.cc )

target_link_libraries(
  XrdSeckrb5
  XrdUtils
  ${KERBEROS5_LIBRARIES} )

set_target_properties(
  XrdSeckrb5
  PROPERTIES
  VERSION   ${XRD_SEC_KRB5_VERSION}
  SOVERSION ${XRD_SEC_KRB5_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdSeckrb5
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
