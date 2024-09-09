
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

  set( XrdHttpSources
    XrdHttp/XrdHttpProtocol.cc        XrdHttp/XrdHttpProtocol.hh
    XrdHttp/XrdHttpSecurity.cc
    XrdHttp/XrdHttpReq.cc             XrdHttp/XrdHttpReq.hh
                                      XrdHttp/XrdHttpSecXtractor.hh
    XrdHttp/XrdHttpExtHandler.cc      XrdHttp/XrdHttpExtHandler.hh
                                      XrdHttp/XrdHttpStatic.hh
                                      XrdHttp/XrdHttpTrace.hh
    XrdHttp/XrdHttpUtils.cc           XrdHttp/XrdHttpUtils.hh
    XrdHttp/XrdHttpReadRangeHandler.cc  XrdHttp/XrdHttpReadRangeHandler.hh
    XrdHttp/XrdHttpChecksumHandler.cc XrdHttp/XrdHttpChecksumHandler.hh
    XrdHttp/XrdHttpChecksum.cc        XrdHttp/XrdHttpChecksum.hh)

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
    PRIVATE
    XrdServer
    XrdUtils
    XrdCrypto
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto )

  target_link_libraries(
    ${MOD_XRD_HTTP}
    PRIVATE
    XrdUtils
    ${LIB_XRD_HTTP_UTILS} )

  set_target_properties(
    ${LIB_XRD_HTTP_UTILS}
    PROPERTIES
    VERSION   ${XRD_HTTP_UTILS_VERSION}
    SOVERSION ${XRD_HTTP_UTILS_SOVERSION})

  set_target_properties(
    ${MOD_XRD_HTTP}
    PROPERTIES
    SUFFIX ".so")

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_HTTP_UTILS} ${MOD_XRD_HTTP}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

endif()
