
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_SEC        XrdSec-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_PROT   XrdSecProt-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_PWD    XrdSecpwd-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_SSS    XrdSecsss-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_UNIX   XrdSecunix-${PLUGIN_VERSION} )

add_dependencies(plugins ${LIB_XRD_SEC}
  ${LIB_XRD_SEC_PROT}
  ${LIB_XRD_SEC_PWD}
  ${LIB_XRD_SEC_SSS}
  ${LIB_XRD_SEC_UNIX})

#-------------------------------------------------------------------------------
# The XrdSec module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_SEC}
  MODULE
                                      XrdSec/XrdSecAttr.hh
  XrdSec/XrdSecClient.cc
                                      XrdSec/XrdSecEntityPin.hh
                                      XrdSec/XrdSecInterface.hh
  XrdSec/XrdSecPManager.cc            XrdSec/XrdSecPManager.hh
  XrdSec/XrdSecProtocolhost.cc        XrdSec/XrdSecProtocolhost.hh
  XrdSec/XrdSecServer.cc              XrdSec/XrdSecServer.hh
  XrdSec/XrdSecTLayer.cc              XrdSec/XrdSecTLayer.hh
  XrdSec/XrdSecTrace.hh )

target_link_libraries(
  ${LIB_XRD_SEC}
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS} )

set_target_properties(
  ${LIB_XRD_SEC}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdSecpwd module
#-------------------------------------------------------------------------------
if( WITH_OPENSSL3 )
  set( XrdSecProtectSources
    XrdSec/openssl3/XrdSecProtect.cc    XrdSec/XrdSecProtect.hh
    XrdSec/XrdSecProtector.cc           XrdSec/XrdSecProtector.hh )
else()
  set( XrdSecProtectSources
    XrdSec/XrdSecProtect.cc             XrdSec/XrdSecProtect.hh
    XrdSec/XrdSecProtector.cc           XrdSec/XrdSecProtector.hh )
endif()

add_library(
  ${LIB_XRD_SEC_PROT}
  MODULE
  ${XrdSecProtectSources} )

if( BUILD_CRYPTO )
  target_link_libraries(
    ${LIB_XRD_SEC_PROT}
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_CRYPTO_LIBRARY} )
else()
  target_link_libraries(
    ${LIB_XRD_SEC_PROT}
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT} )
endif()

set_target_properties(
  ${LIB_XRD_SEC_PROT}
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
  ${CMAKE_THREAD_LIBS_INIT}
  ${CRYPT_LIBRARY} )

set_target_properties(
  ${LIB_XRD_SEC_PWD}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

if( NOT XRDCL_LIB_ONLY )
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
endif()

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

if( NOT XRDCL_LIB_ONLY )
#-------------------------------------------------------------------------------
# xrdsssadmin
#-------------------------------------------------------------------------------
add_executable(
  xrdsssadmin
  XrdSecsss/XrdSecsssAdmin.cc )

target_link_libraries(
  xrdsssadmin
  XrdUtils )
endif()

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
  ${LIB_XRD_SEC} ${LIB_XRD_SEC_PWD} ${LIB_XRD_SEC_SSS} ${LIB_XRD_SEC_UNIX} ${LIB_XRD_SEC_PROT}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

if( NOT XRDCL_LIB_ONLY )
install(
  TARGETS
  xrdsssadmin xrdpwdadmin
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdsssadmin.8
  ${PROJECT_SOURCE_DIR}/docs/man/xrdpwdadmin.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 )
endif()
