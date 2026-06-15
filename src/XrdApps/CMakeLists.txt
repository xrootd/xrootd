add_library(XrdAppUtils SHARED
  XrdCpConfig.cc  XrdCpConfig.hh
  XrdCpFile.cc    XrdCpFile.hh
  XrdMpxXml.cc    XrdMpxXml.hh
)

target_link_libraries(XrdAppUtils PRIVATE XrdUtils)

set_target_properties(XrdAppUtils
  PROPERTIES
    SOVERSION ${XRootD_VERSION_MAJOR}
    VERSION ${XRootD_LIBVERSION}
)

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

find_package(CLI11 1.8.0 QUIET)
if(NOT CLI11_FOUND)
  include(FetchContent)
  FetchContent_Declare(CLI11
    URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.2.tar.gz
    URL_HASH SHA256=f2d893a65c3b1324c50d4e682c0cdc021dd0477ae2c048544f39eed6654b699a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(CLI11)
endif()

add_executable(xrd XrdCli.cc)
set_target_properties(xrd PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED TRUE
)
target_link_libraries(xrd PRIVATE
  CLI11::CLI11
)

install(
  TARGETS
    xrd
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
