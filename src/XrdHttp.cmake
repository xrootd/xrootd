include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_HTTP_UTILS XrdHttpUtils )
set( MOD_XRD_HTTP       XrdHttp-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_HTTP_UTILS_VERSION   2.0.0 )
set( XRD_HTTP_UTILS_SOVERSION 2 )

if( BUILD_HTTP )
  #-----------------------------------------------------------------------------
  # The XrdHttp library
  #-----------------------------------------------------------------------------
  include_directories( ${OPENSSL_INCLUDE_DIR} )

  if( WITH_OPENSSL3 )
    set( XrdHttpSources
      XrdHttp/XrdHttpProtocol.cc        XrdHttp/XrdHttpProtocol.hh
      XrdHttp/XrdHttpSecurity.cc
      XrdHttp/XrdHttpReq.cc             XrdHttp/XrdHttpReq.hh
                                        XrdHttp/XrdHttpSecXtractor.hh
      XrdHttp/XrdHttpExtHandler.cc      XrdHttp/XrdHttpExtHandler.hh
                                        XrdHttp/XrdHttpStatic.hh
                                        XrdHttp/XrdHttpTrace.hh
      XrdHttp/openssl3/XrdHttpUtils.cc  XrdHttp/XrdHttpUtils.hh )
  else()
    set( XrdHttpSources
      XrdHttp/XrdHttpProtocol.cc        XrdHttp/XrdHttpProtocol.hh
      XrdHttp/XrdHttpSecurity.cc
      XrdHttp/XrdHttpReq.cc             XrdHttp/XrdHttpReq.hh
                                        XrdHttp/XrdHttpSecXtractor.hh
      XrdHttp/XrdHttpExtHandler.cc      XrdHttp/XrdHttpExtHandler.hh
                                        XrdHttp/XrdHttpStatic.hh
                                        XrdHttp/XrdHttpTrace.hh
      XrdHttp/XrdHttpUtils.cc           XrdHttp/XrdHttpUtils.hh )
  endif()

  # Note this is marked as a shared library as XrdHttp plugins are expected to
  # link against this for the XrdHttpExt class implementations.
  add_library(
    ${LIB_XRD_HTTP_UTILS}
    SHARED
    ${XrdHttpSources} )

  add_library(
    ${MOD_XRD_HTTP}
    MODULE
    XrdHttp/XrdHttpModule.cc )

  target_link_libraries(
    ${LIB_XRD_HTTP_UTILS}
    XrdServer
    XrdUtils
    XrdCrypto
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARY} )

  target_link_libraries(
    ${MOD_XRD_HTTP}
    XrdUtils
    ${LIB_XRD_HTTP_UTILS} )

  set_target_properties(
    ${LIB_XRD_HTTP_UTILS}
    PROPERTIES
    VERSION   ${XRD_HTTP_UTILS_VERSION}
    SOVERSION ${XRD_HTTP_UTILS_SOVERSION}
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES "" )

  set_target_properties(
    ${MOD_XRD_HTTP}
    PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    SUFFIX ".so"
    LINK_INTERFACE_LIBRARIES "" )

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_HTTP_UTILS} ${MOD_XRD_HTTP}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

endif()
