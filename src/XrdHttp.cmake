
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_HTTP_VERSION   1.0.0 )
set( XRD_HTTP_SOVERSION 1 )

if( BUILD_CRYPTO )
  #-----------------------------------------------------------------------------
  # The XrdHttp library
  #-----------------------------------------------------------------------------
  include_directories( ${OPENSSL_INCLUDE_DIR} )

  add_library(
    XrdHttp
    SHARED
    XrdHttp/XrdHttpProtocol.cc    XrdHttp/XrdHttpProtocol.hh
    XrdHttp/XrdHttpReq.cc         XrdHttp/XrdHttpReq.hh
                                  XrdHttp/XrdHttpSecXtractor.hh
                                  XrdHttp/XrdHttpStatic.hh
    XrdHttp/XrdHttpTrace.cc       XrdHttp/XrdHttpTrace.hh
    XrdHttp/XrdHttpUtils.cc       XrdHttp/XrdHttpUtils.hh )

  target_link_libraries(
    XrdHttp
    XrdServer
    XrdXrootd
    XrdUtils
    XrdCrypto
    dl
    pthread
    ${OPENSSL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARY} )

  set_target_properties(
    XrdHttp
    PROPERTIES
    VERSION   ${XRD_HTTP_VERSION}
    SOVERSION ${XRD_HTTP_SOVERSION}
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES "" )

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS XrdHttp
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

endif()