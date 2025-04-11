set( XrdSecgsi          XrdSecgsi-${PLUGIN_VERSION} )
set( XrdSecgsi_GMAPDN   XrdSecgsiGMAPDN-${PLUGIN_VERSION} )
set( XrdSecgsi_AUTHZVO  XrdSecgsiAUTHZVO-${PLUGIN_VERSION} )

add_library(${XrdSecgsi} MODULE
  XrdSecProtocolgsi.cc  XrdSecProtocolgsi.hh
                        XrdSecgsiOpts.hh
                        XrdSecgsiTrace.hh
)

target_link_libraries(${XrdSecgsi} PRIVATE XrdCrypto XrdUtils)

add_library(${XrdSecgsi_AUTHZVO} MODULE XrdSecgsiAuthzFunVO.cc)
target_link_libraries(${XrdSecgsi_AUTHZVO} PRIVATE XrdUtils)

add_library(${XrdSecgsi_GMAPDN} MODULE XrdSecgsiGMAPFunDN.cc)
target_link_libraries(${XrdSecgsi_GMAPDN} PRIVATE XrdUtils)

add_dependencies(plugins ${XrdSecgsi} ${XrdSecgsi_GMAPDN} ${XrdSecgsi_AUTHZVO})

install(
  TARGETS
    ${XrdSecgsi}
    ${XrdSecgsi_AUTHZVO}
    ${XrdSecgsi_GMAPDN}
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(NOT XRDCL_LIB_ONLY)
  add_executable(xrdgsiproxy XrdSecgsiProxy.cc)
  target_link_libraries(xrdgsiproxy XrdCrypto XrdUtils OpenSSL::Crypto)

  add_executable(xrdgsitest XrdSecgsitest.cc)
  target_link_libraries(xrdgsitest XrdCrypto XrdUtils OpenSSL::Crypto)

  install(
    TARGETS
      xrdgsiproxy
      xrdgsitest
    RUNTIME
      DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

