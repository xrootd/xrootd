
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SEC_GSI_VERSION            3.0.0 )
set( XRD_SEC_GSI_SOVERSION          3 )

set( XRD_SEC_GSI_GMAPLDAP_VERSION   2.0.0 )
set( XRD_SEC_GSI_GMAPLDAP_SOVERSION 2 )

set( XRD_SEC_GSI_GMAPDN_VERSION     2.0.0 )
set( XRD_SEC_GSI_GMAPDN_SOVERSION   2 )

set( XRD_SEC_GSI_AUTHZVO_VERSION    2.0.0 )
set( XRD_SEC_GSI_AUTHZVO_SOVERSION  2 )

#-------------------------------------------------------------------------------
# The XrdSecgsi library
#-------------------------------------------------------------------------------
add_library(
  XrdSecgsi
  SHARED
  XrdSecgsi/XrdSecProtocolgsi.cc      XrdSecgsi/XrdSecProtocolgsi.hh
                                      XrdSecgsi/XrdSecgsiTrace.hh )

target_link_libraries(
  XrdSecgsi
  XrdCryptossl
  XrdCrypto
  XrdUtils
  pthread )

set_target_properties(
  XrdSecgsi
  PROPERTIES
  VERSION   ${XRD_SEC_GSI_VERSION}
  SOVERSION ${XRD_SEC_GSI_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdSecgsiGMAPLDAP library
#-------------------------------------------------------------------------------
add_library(
  XrdSecgsiGMAPLDAP
  SHARED
  XrdSecgsi/XrdSecgsiGMAPFunLDAP.cc )

#target_link_libraries(
#  XrdSecgsiGMAPLDAP
#  XrdSecgsi )

set_target_properties(
  XrdSecgsiGMAPLDAP
  PROPERTIES
  VERSION   ${XRD_SEC_GSI_GMAPLDAP_VERSION}
  SOVERSION ${XRD_SEC_GSI_GMAPLDAP_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdSecgsiAuthzVO library
#-------------------------------------------------------------------------------
add_library(
  XrdSecgsiAuthzVO
  SHARED
  XrdSecgsi/XrdSecgsiAuthzFunVO.cc )

target_link_libraries(
  XrdSecgsiAuthzVO
  XrdUtils )

set_target_properties(
  XrdSecgsiAuthzVO
  PROPERTIES
  VERSION   ${XRD_SEC_GSI_AUTHZVO_VERSION}
  SOVERSION ${XRD_SEC_GSI_AUTHZVO_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdSecgsiGMAPDN library
#-------------------------------------------------------------------------------
add_library(
  XrdSecgsiGMAPDN
  SHARED
  XrdSecgsi/XrdSecgsiGMAPFunDN.cc )

target_link_libraries(
  XrdSecgsiGMAPDN
  XrdSecgsi
  XrdUtils )

set_target_properties(
  XrdSecgsiGMAPDN
  PROPERTIES
  VERSION   ${XRD_SEC_GSI_GMAPDN_VERSION}
  SOVERSION ${XRD_SEC_GSI_GMAPDN_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# xrdgsiproxy
#-------------------------------------------------------------------------------
add_executable(
  xrdgsiproxy
  XrdSecgsi/XrdSecgsiProxy.cc )

target_link_libraries(
  xrdgsiproxy
  XrdCryptossl
  XrdCrypto
  XrdUtils )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdSecgsi XrdSecgsiGMAPDN XrdSecgsiGMAPLDAP xrdgsiproxy
          XrdSecgsiAuthzVO
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdgsiproxy.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )
