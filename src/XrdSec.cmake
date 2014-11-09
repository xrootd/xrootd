
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_SEC        XrdSec-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_PWD    XrdSecpwd-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_SSS    XrdSecsss-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_UNIX   XrdSecunix-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# The XrdSec module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC}
  MODULE
  XrdSec/XrdSecClient.cc
                                      XrdSec/XrdSecEntity.hh
                                      XrdSec/XrdSecInterface.hh
  XrdSec/XrdSecPManager.cc            XrdSec/XrdSecPManager.hh
  XrdSec/XrdSecProtocolhost.cc        XrdSec/XrdSecProtocolhost.hh
  XrdSec/XrdSecServer.cc              XrdSec/XrdSecServer.hh
  XrdSec/XrdSecTLayer.cc              XrdSec/XrdSecTLayer.hh
  XrdSec/XrdSecTrace.hh )

target_link_libraries(
  ${LIB_XRD_SEC}
  XrdUtils
  pthread
  dl )

set_target_properties(
  ${LIB_XRD_SEC}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdSecpwd module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_PWD}
  MODULE
  XrdSecpwd/XrdSecProtocolpwd.cc      XrdSecpwd/XrdSecProtocolpwd.hh
                                      XrdSecpwd/XrdSecpwdPlatform.hh )

target_link_libraries(
  ${LIB_XRD_SEC_PWD}
  XrdCrypto
  XrdUtils
  pthread
  ${CRYPT_LIBRARY} )

set_target_properties(
  ${LIB_XRD_SEC_PWD}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# xrdpwdadmin
#-------------------------------------------------------------------------------
add_executable(
  xrdpwdadmin
  XrdSecpwd/XrdSecpwdSrvAdmin.cc )

target_link_libraries(
  xrdpwdadmin
  XrdCrypto
  XrdUtils )

#-------------------------------------------------------------------------------
# The XrdSecsss module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_SSS}
  MODULE
  XrdSecsss/XrdSecProtocolsss.cc   XrdSecsss/XrdSecProtocolsss.hh
                                   XrdSecsss/XrdSecsssRR.hh )

target_link_libraries(
  ${LIB_XRD_SEC_SSS}
  XrdCryptoLite
  XrdUtils )

set_target_properties(
  ${LIB_XRD_SEC_SSS}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# xrdsssadmin
#-------------------------------------------------------------------------------
add_executable(
  xrdsssadmin
  XrdSecsss/XrdSecsssAdmin.cc )

target_link_libraries(
  xrdsssadmin
  XrdUtils )

#-------------------------------------------------------------------------------
# The XrdSecunix module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC_UNIX}
  MODULE
  XrdSecunix/XrdSecProtocolunix.cc )

target_link_libraries(
  ${LIB_XRD_SEC_UNIX}
  XrdUtils )

set_target_properties(
  ${LIB_XRD_SEC_UNIX}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS
  ${LIB_XRD_SEC} ${LIB_XRD_SEC_PWD} ${LIB_XRD_SEC_SSS} ${LIB_XRD_SEC_UNIX}
  xrdsssadmin xrdpwdadmin
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdsssadmin.8
  ${PROJECT_SOURCE_DIR}/docs/man/xrdpwdadmin.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 )
