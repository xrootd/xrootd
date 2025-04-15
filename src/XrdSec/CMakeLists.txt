set(XrdSec     XrdSec-${PLUGIN_VERSION})
set(XrdSecProt XrdSecProt-${PLUGIN_VERSION})

target_sources(XrdUtils
  PRIVATE
    XrdSecEntity.cc        XrdSecEntity.hh
    XrdSecEntityAttr.cc    XrdSecEntityAttr.hh
    XrdSecEntityXtra.cc    XrdSecEntityXtra.hh
    XrdSecLoadSecurity.cc  XrdSecLoadSecurity.hh
                           XrdSecMonitor.hh
)

add_library(${XrdSec} MODULE
                         XrdSecAttr.hh
  XrdSecClient.cc
                         XrdSecEntityPin.hh
                         XrdSecInterface.hh
  XrdSecPManager.cc      XrdSecPManager.hh
  XrdSecProtocolhost.cc  XrdSecProtocolhost.hh
  XrdSecServer.cc        XrdSecServer.hh
  XrdSecTLayer.cc        XrdSecTLayer.hh
  XrdSecTrace.hh
)

add_library(${XrdSecProt} MODULE
  XrdSecProtect.cc    XrdSecProtect.hh
  XrdSecProtector.cc  XrdSecProtector.hh
)

target_link_libraries(${XrdSec} PRIVATE XrdUtils)
target_link_libraries(${XrdSecProt} PRIVATE XrdUtils OpenSSL::Crypto)

add_dependencies(plugins ${XrdSec} ${XrdSecProt})

install(
  TARGETS
    ${XrdSec}
    ${XrdSecProt}
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
