

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
  PRIVATE
  XrdCrypto
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT} )

#-------------------------------------------------------------------------------
# The XrdSecgsiAuthzVO module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_GSI_AUTHZVO}
  MODULE
  XrdSecgsi/XrdSecgsiAuthzFunVO.cc )

target_link_libraries(
  ${LIB_XRD_SEC_GSI_AUTHZVO}
  PRIVATE
  XrdUtils )

#-------------------------------------------------------------------------------
# The XrdSecgsiGMAPDN module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_GSI_GMAPDN}
  MODULE
  XrdSecgsi/XrdSecgsiGMAPFunDN.cc )

target_link_libraries(
  ${LIB_XRD_SEC_GSI_GMAPDN}
  PRIVATE
  XrdUtils )

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
  OpenSSL::Crypto )

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
  OpenSSL::Crypto )
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
endif()

