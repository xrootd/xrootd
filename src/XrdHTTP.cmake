
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
    XrdHTTP/XrdHttpProtocol.cc    XrdHTTP/XrdHttpProtocol.hh
    XrdHTTP/XrdHttpReq.cc         XrdHTTP/XrdHttpReq.hh
                                  XrdHTTP/XrdHttpSecXtractor.hh
                                  XrdHTTP/XrdHttpStatic.hh
    XrdHTTP/XrdHttpTrace.cc       XrdHTTP/XrdHttpTrace.hh
    XrdHTTP/XrdHttpUtils.cc       XrdHTTP/XrdHttpUtils.hh )

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