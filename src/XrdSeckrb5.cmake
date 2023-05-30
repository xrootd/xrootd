include_directories( ${KERBEROS5_INCLUDE_DIR} )

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
  PRIVATE
  XrdUtils
  ${KERBEROS5_LIBRARIES} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_SEC_KRB5}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
