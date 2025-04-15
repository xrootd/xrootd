add_library(XrdAppUtils SHARED
  XrdCpConfig.cc  XrdCpConfig.hh
  XrdCpFile.cc    XrdCpFile.hh
  XrdMpxXml.cc    XrdMpxXml.hh
)

target_link_libraries(XrdAppUtils PRIVATE XrdUtils)

set_target_properties(XrdAppUtils
  PROPERTIES VERSION 2.0.0 SOVERSION 2)

set(XrdClProxyPlugin XrdClProxyPlugin-${PLUGIN_VERSION})

add_library(${XrdClProxyPlugin} MODULE
  XrdClProxyPlugin/ProxyPrefixPlugin.cc
  XrdClProxyPlugin/ProxyPrefixFile.cc
)

target_link_libraries(${XrdClProxyPlugin} PRIVATE XrdCl)

set(XrdClRecorder XrdClRecorder-${PLUGIN_VERSION})

add_library(${XrdClRecorder} MODULE
  XrdClRecordPlugin/XrdClRecorderPlugin.cc
)

target_link_libraries(${XrdClRecorder} PRIVATE XrdCl)

add_executable(xrdreplay XrdClRecordPlugin/XrdClReplay.cc)

target_link_libraries(xrdreplay
  XrdCl
  XrdUtils 
  ${CMAKE_THREAD_LIBS_INIT}
)

install(
  TARGETS
    xrdreplay
    XrdAppUtils
    ${XrdClProxyPlugin}
    ${XrdClRecorder}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(NOT XRDCL_ONLY)
  add_executable(cconfig XrdAppsCconfig.cc)
  target_link_libraries(cconfig XrdUtils)

  add_executable(mpxstats XrdMpxStats.cc)
  target_link_libraries(
    mpxstats
    XrdAppUtils
    XrdUtils
    ${EXTRA_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${SOCKET_LIBRARY}
  )

  add_executable(wait41 XrdWait41.cc)
  target_link_libraries(wait41
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBS}
  )

  add_executable(xrdacctest XrdAccTest.cc)
  target_link_libraries(xrdacctest XrdServer XrdUtils)

  add_executable(xrdadler32 Xrdadler32.cc)
  target_link_libraries(xrdadler32
    XrdPosix
    XrdUtils
    ZLIB::ZLIB
    ${CMAKE_THREAD_LIBS_INIT}
  )

  add_executable(xrdcks XrdCks.cc)
  target_link_libraries(xrdcks XrdUtils)

  add_executable(xrdcrc32c XrdCrc32c.cc)
  target_link_libraries(xrdcrc32c XrdUtils)

  add_executable(xrdmapc XrdMapCluster.cc)
  target_link_libraries(xrdmapc XrdCl XrdUtils)

  add_executable(xrdpinls XrdPinls.cc)

  add_executable(xrdprep XrdPrep.cc)
  target_link_libraries(xrdprep XrdCl XrdUtils)

  add_executable(xrdqstats XrdQStats.cc)
  target_link_libraries(xrdqstats
    XrdCl
    XrdAppUtils
    XrdUtils
    ${EXTRA_LIBS}
  )

  install(
    TARGETS
      cconfig
      mpxstats
      wait41
      xrdacctest
      xrdadler32
      xrdcks
      xrdcrc32c
      xrdmapc
      xrdpinls
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()
