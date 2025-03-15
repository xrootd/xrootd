if(NOT BUILD_HTTP)
  return()
endif()

# Note that XrdHttpUtils is marked as a shared library as XrdHttp plugins
# are expected to link against it for the XrdHttpExt class implementations.

add_library(XrdHttpUtils SHARED
  XrdHttpProtocol.cc         XrdHttpProtocol.hh
  XrdHttpSecurity.cc
  XrdHttpReq.cc              XrdHttpReq.hh
                             XrdHttpSecXtractor.hh
  XrdHttpExtHandler.cc       XrdHttpExtHandler.hh
                             XrdHttpStatic.hh
                             XrdHttpTrace.hh
  XrdHttpUtils.cc            XrdHttpUtils.hh
  XrdHttpReadRangeHandler.cc XrdHttpReadRangeHandler.hh
  XrdHttpChecksumHandler.cc  XrdHttpChecksumHandler.hh
  XrdHttpChecksum.cc         XrdHttpChecksum.hh
)

set_target_properties(XrdHttpUtils
  PROPERTIES VERSION 2.0.0 SOVERSION 2)

target_link_libraries(XrdHttpUtils
  PRIVATE
    XrdServer
    XrdUtils
    XrdCrypto
  PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
)

set(XrdHttp XrdHttp-${PLUGIN_VERSION})

add_library(${XrdHttp} MODULE XrdHttpModule.cc)

target_link_libraries(${XrdHttp} PRIVATE XrdUtils XrdHttpUtils)

set_target_properties(${XrdHttp} PROPERTIES SUFFIX ".so")

install(TARGETS ${XrdHttp} XrdHttpUtils LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
