

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRDCL_PROXY_PLUGIN XrdClProxyPlugin-${PLUGIN_VERSION} )
set( LIB_XRDCL_RECORDER_PLUGIN XrdClRecorder-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_APP_UTILS_VERSION   2.0.0 )
set( XRD_APP_UTILS_SOVERSION 2 )

#-------------------------------------------------------------------------------
# This part is NOT built for XrdCl only builds
#-------------------------------------------------------------------------------
if( NOT XRDCL_ONLY )

  #-----------------------------------------------------------------------------
  # xrdadler32
  #-----------------------------------------------------------------------------
  add_executable(
    xrdadler32
    XrdApps/Xrdadler32.cc )

  target_link_libraries(
    xrdadler32
    XrdPosix
    XrdUtils
    ZLIB::ZLIB
    ${CMAKE_THREAD_LIBS_INIT})

  #-----------------------------------------------------------------------------
  # xrdcks
  #-----------------------------------------------------------------------------
  add_executable(
    xrdcks
    XrdApps/XrdCks.cc )

  target_link_libraries(
    xrdcks
    XrdUtils )

  #-----------------------------------------------------------------------------
  # xrdcrc32c
  #-----------------------------------------------------------------------------
  add_executable(
    xrdcrc32c
    XrdApps/XrdCrc32c.cc )

  target_link_libraries(
    xrdcrc32c
    XrdUtils )

  #-----------------------------------------------------------------------------
  # cconfig
  #-----------------------------------------------------------------------------
  add_executable(
    cconfig
    XrdApps/XrdAppsCconfig.cc )

  target_link_libraries(
    cconfig
    XrdUtils )

  #-----------------------------------------------------------------------------
  # mpxstats
  #-----------------------------------------------------------------------------
  add_executable(
    mpxstats
    XrdApps/XrdMpxStats.cc )

  target_link_libraries(
    mpxstats
    XrdAppUtils
    XrdUtils
    ${EXTRA_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${SOCKET_LIBRARY} )

  #-----------------------------------------------------------------------------
  # xrdprep
  #-----------------------------------------------------------------------------
  add_executable(
    xrdprep
    XrdApps/XrdPrep.cc )

  target_link_libraries(
    xrdprep
    XrdCl
    XrdUtils )

  #-----------------------------------------------------------------------------
  # wait41
  #-----------------------------------------------------------------------------
  add_executable(
    wait41
    XrdApps/XrdWait41.cc )

  target_link_libraries(
    wait41
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBS} )

  #-----------------------------------------------------------------------------
  # xrdacctest
  #-----------------------------------------------------------------------------
  add_executable(
    xrdacctest
    XrdApps/XrdAccTest.cc )

  target_link_libraries(
    xrdacctest
    XrdServer
    XrdUtils )

  #-------------------------------------------------------------------------------
  # xrdmapc
  #-------------------------------------------------------------------------------
  add_executable(
    xrdmapc
    XrdApps/XrdMapCluster.cc )

  target_link_libraries(
    xrdmapc
    XrdCl
    XrdUtils )

  #-------------------------------------------------------------------------------
  # xrdpinls
  #-------------------------------------------------------------------------------
  add_executable(
    xrdpinls
    XrdApps/XrdPinls.cc )

  #-------------------------------------------------------------------------------
  # xrdqstats
  #-------------------------------------------------------------------------------
  add_executable(
    xrdqstats
    XrdApps/XrdQStats.cc )

  target_link_libraries(
    xrdqstats
    XrdCl
    XrdAppUtils
    XrdUtils
    ${EXTRA_LIBS} )
endif()

#-------------------------------------------------------------------------------
# AppUtils
#-------------------------------------------------------------------------------
add_library(
  XrdAppUtils
  SHARED
  XrdApps/XrdCpConfig.cc          XrdApps/XrdCpConfig.hh
  XrdApps/XrdCpFile.cc            XrdApps/XrdCpFile.hh
  XrdApps/XrdMpxXml.cc            XrdApps/XrdMpxXml.hh )

target_link_libraries(
  XrdAppUtils
  PRIVATE
  XrdUtils )

set_target_properties(
  XrdAppUtils
  PROPERTIES
  VERSION   ${XRD_APP_UTILS_VERSION}
  SOVERSION ${XRD_APP_UTILS_SOVERSION} )

#-------------------------------------------------------------------------------
# XrdClProxyPlugin library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRDCL_PROXY_PLUGIN}
  MODULE
  XrdApps/XrdClProxyPlugin/ProxyPrefixPlugin.cc
  XrdApps/XrdClProxyPlugin/ProxyPrefixFile.cc)

target_link_libraries(${LIB_XRDCL_PROXY_PLUGIN} PRIVATE XrdCl)

#-------------------------------------------------------------------------------
# XrdClRecorder library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRDCL_RECORDER_PLUGIN}
  MODULE
  XrdApps/XrdClRecordPlugin/XrdClRecorderPlugin.cc )

target_link_libraries(${LIB_XRDCL_RECORDER_PLUGIN} PRIVATE XrdCl)

add_executable(
  xrdreplay
  XrdApps/XrdClRecordPlugin/XrdClReplay.cc )

target_link_libraries(
  xrdreplay
  ${CMAKE_THREAD_LIBS_INIT}
  XrdCl
  XrdUtils )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdAppUtils ${LIB_XRDCL_PROXY_PLUGIN} ${LIB_XRDCL_RECORDER_PLUGIN}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

if( NOT XRDCL_ONLY )
  install(
    TARGETS xrdacctest xrdadler32 cconfig mpxstats wait41 xrdmapc xrdpinls xrdcrc32c xrdcks
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
endif()

install(
  TARGETS xrdreplay
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
