include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_HTTP       XrdHttp-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

if( BUILD_HTTP )
  #-----------------------------------------------------------------------------
  # The XrdHttp library
  #-----------------------------------------------------------------------------
  include_directories( ${OPENSSL_INCLUDE_DIR} )

  add_library(
    ${LIB_XRD_HTTP}
    MODULE
    XrdHttp/XrdHttpProtocol.cc    XrdHttp/XrdHttpProtocol.hh
    XrdHttp/XrdHttpReq.cc         XrdHttp/XrdHttpReq.hh
                                  XrdHttp/XrdHttpSecXtractor.hh
                                  XrdHttp/XrdHttpStatic.hh
    XrdHttp/XrdHttpTrace.cc       XrdHttp/XrdHttpTrace.hh
    XrdHttp/XrdHttpUtils.cc       XrdHttp/XrdHttpUtils.hh )

  target_link_libraries(
    ${LIB_XRD_HTTP}
    XrdServer
    XrdUtils
    XrdCrypto
    dl
    pthread
    ${OPENSSL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARY} )

  set_target_properties(
    ${LIB_XRD_HTTP}
    PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES "" )

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_HTTP}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

endif()
