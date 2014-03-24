
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SEC_VERSION        1.0.0 )
set( XRD_SEC_SOVERSION      1 )
set( XRD_SEC_PWD_VERSION    2.0.0 )
set( XRD_SEC_PWD_SOVERSION  2 )
set( XRD_SEC_SSS_VERSION    2.0.0 )
set( XRD_SEC_SSS_SOVERSION  2 )
set( XRD_SEC_UNIX_VERSION   2.0.0 )
set( XRD_SEC_UNIX_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdSec library
#-------------------------------------------------------------------------------
add_library(
  XrdSec
  SHARED
  XrdSec/XrdSecClient.cc
                                      XrdSec/XrdSecEntity.hh
                                      XrdSec/XrdSecInterface.hh
  XrdSec/XrdSecLoader.cc              XrdSec/XrdSecLoader.hh
  XrdSec/XrdSecPManager.cc            XrdSec/XrdSecPManager.hh
  XrdSec/XrdSecProtocolhost.cc        XrdSec/XrdSecProtocolhost.hh
  XrdSec/XrdSecServer.cc              XrdSec/XrdSecServer.hh
  XrdSec/XrdSecTLayer.cc              XrdSec/XrdSecTLayer.hh
  XrdSec/XrdSecTrace.hh )

target_link_libraries(
  XrdSec
  XrdUtils
  pthread
  dl )

set_target_properties(
  XrdSec
  PROPERTIES
  VERSION   ${XRD_SEC_VERSION}
  SOVERSION ${XRD_SEC_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

# FIXME: test
#-rw-r--r-- 1 ljanyst ljanyst  5806 2011-03-21 16:13 XrdSectestClient.cc
#-rw-r--r-- 1 ljanyst ljanyst 10758 2011-03-21 16:13 XrdSectestServer.cc

#-------------------------------------------------------------------------------
# The XrdSecpwd library
#-------------------------------------------------------------------------------
add_library(
  XrdSecpwd
  SHARED
  XrdSecpwd/XrdSecProtocolpwd.cc      XrdSecpwd/XrdSecProtocolpwd.hh
                                      XrdSecpwd/XrdSecpwdPlatform.hh )

target_link_libraries(
  XrdSecpwd
  XrdCrypto
  XrdUtils
  pthread
  ${CRYPT_LIBRARY} )

set_target_properties(
  XrdSecpwd
  PROPERTIES
  VERSION   ${XRD_SEC_PWD_VERSION}
  SOVERSION ${XRD_SEC_PWD_SOVERSION}
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
# The XrdSecsss library
#-------------------------------------------------------------------------------
add_library(
  XrdSecsss
  SHARED
  XrdSecsss/XrdSecProtocolsss.cc   XrdSecsss/XrdSecProtocolsss.hh
  XrdSecsss/XrdSecsssID.cc         XrdSecsss/XrdSecsssID.hh
  XrdSecsss/XrdSecsssKT.cc         XrdSecsss/XrdSecsssKT.hh
                                   XrdSecsss/XrdSecsssRR.hh )

target_link_libraries(
  XrdSecsss
  XrdCryptoLite
  XrdUtils )

set_target_properties(
  XrdSecsss
  PROPERTIES
  VERSION   ${XRD_SEC_SSS_VERSION}
  SOVERSION ${XRD_SEC_SSS_SOVERSION}
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
  XrdSecsss
  XrdUtils )

#-------------------------------------------------------------------------------
# The XrdSecunix library
#-------------------------------------------------------------------------------
add_library(
  XrdSecunix
  SHARED
  XrdSecunix/XrdSecProtocolunix.cc )

target_link_libraries(
  XrdSecunix
  XrdUtils )

set_target_properties(
  XrdSecunix
  PROPERTIES
  VERSION   ${XRD_SEC_UNIX_VERSION}
  SOVERSION ${XRD_SEC_UNIX_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdSec XrdSecpwd XrdSecsss XrdSecunix xrdsssadmin xrdpwdadmin
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdsssadmin.8
  ${PROJECT_SOURCE_DIR}/docs/man/xrdpwdadmin.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 )
