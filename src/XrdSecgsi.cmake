
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( LIB_XRD_SEC_GSI          XrdSecgsi-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_GSI_GMAPDN   XrdSecgsiGMAPDN-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_GSI_AUTHZVO  XrdSecgsiAUTHZVO-${PLUGIN_VERSION} )

add_dependencies(plugins ${LIB_XRD_SEC_GSI}
  ${LIB_XRD_SEC_GSI_GMAPDN}
  ${LIB_XRD_SEC_GSI_AUTHZVO})

#-------------------------------------------------------------------------------
# The XrdSecgsi library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_GSI}
  MODULE
  XrdSecgsi/XrdSecProtocolgsi.cc      XrdSecgsi/XrdSecProtocolgsi.hh
                                      XrdSecgsi/XrdSecgsiOpts.hh
                                      XrdSecgsi/XrdSecgsiTrace.hh )

target_link_libraries(
  ${LIB_XRD_SEC_GSI}
  XrdCrypto
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT} )

set_target_properties(
  ${LIB_XRD_SEC_GSI}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdSecgsiAuthzVO module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_GSI_AUTHZVO}
  MODULE
  XrdSecgsi/XrdSecgsiAuthzFunVO.cc )

target_link_libraries(
  ${LIB_XRD_SEC_GSI_AUTHZVO}
  XrdUtils )

set_target_properties(
  ${LIB_XRD_SEC_GSI_AUTHZVO}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdSecgsiGMAPDN module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_GSI_GMAPDN}
  MODULE
  XrdSecgsi/XrdSecgsiGMAPFunDN.cc )

target_link_libraries(
  ${LIB_XRD_SEC_GSI_GMAPDN}
  XrdUtils )

set_target_properties(
  ${LIB_XRD_SEC_GSI_GMAPDN}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

if( NOT XRDCL_LIB_ONLY )
#-------------------------------------------------------------------------------
# xrdgsiproxy
#-------------------------------------------------------------------------------
add_executable(
  xrdgsiproxy
  XrdSecgsi/XrdSecgsiProxy.cc )

target_link_libraries(
  xrdgsiproxy
  XrdCrypto
  XrdUtils
  ${OPENSSL_CRYPTO_LIBRARY} )

#-------------------------------------------------------------------------------
# xrdgsitest
#-------------------------------------------------------------------------------
add_executable(
  xrdgsitest
  XrdSecgsi/XrdSecgsitest.cc )

target_link_libraries(
  xrdgsitest
  XrdCrypto
  XrdUtils
  ${OPENSSL_CRYPTO_LIBRARY} )
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS
  ${LIB_XRD_SEC_GSI}
  ${LIB_XRD_SEC_GSI_AUTHZVO}
  ${LIB_XRD_SEC_GSI_GMAPDN}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

if( NOT XRDCL_LIB_ONLY )
install(
  TARGETS
  xrdgsiproxy
  xrdgsitest
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdgsiproxy.1
  ${PROJECT_SOURCE_DIR}/docs/man/xrdgsitest.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

endif()

