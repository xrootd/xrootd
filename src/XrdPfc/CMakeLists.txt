set(XrdPfc                XrdPfc-${PLUGIN_VERSION})
set(XrdFileCache          XrdFileCache-${PLUGIN_VERSION})
set(XrdBlacklistDecision  XrdBlacklistDecision-${PLUGIN_VERSION})
set(XrdPfcPurgeQuota      XrdPfcPurgeQuota-${PLUGIN_VERSION})

add_library(${XrdPfc} MODULE
  XrdPfcTypes.hh
  XrdPfc.cc                 XrdPfc.hh
  XrdPfcConfiguration.cc
  XrdPfcDirStateBase.hh
  XrdPfcDirState.cc         XrdPfcDirState.hh
  XrdPfcDirStateSnapshot.cc XrdPfcDirStateSnapshot.hh
  XrdPfcFPurgeState.cc      XrdPfcFPurgeState.hh
  XrdPfcPurge.cc
  XrdPfcPurgePin.hh
  XrdPfcDirStatePurgeshot.hh
  XrdPfcResourceMonitor.cc  XrdPfcResourceMonitor.hh
  XrdPfcPathParseTools.hh
  XrdPfcFsTraversal.cc      XrdPfcFsTraversal.hh
  XrdPfcCommand.cc
  XrdPfcFile.cc             XrdPfcFile.hh
  XrdPfcFSctl.cc            XrdPfcFSctl.hh
  XrdPfcStats.hh
  XrdPfcInfo.cc             XrdPfcInfo.hh
  XrdPfcIO.cc               XrdPfcIO.hh
  XrdPfcIOFile.cc           XrdPfcIOFile.hh
  XrdPfcIOFileBlock.cc      XrdPfcIOFileBlock.hh
  XrdPfcDecision.hh
)

target_link_libraries(${XrdPfc}
  PRIVATE
    XrdCl
    XrdUtils
    XrdServer
)

add_library(${XrdBlacklistDecision} MODULE XrdPfcBlacklistDecision.cc)

target_link_libraries(${XrdBlacklistDecision} PRIVATE XrdUtils)

add_library(${XrdPfcPurgeQuota} MODULE XrdPfcPurgeQuota.cc)

target_link_libraries(${XrdPfcPurgeQuota} PRIVATE XrdUtils)

add_executable(xrdpfc_print
  XrdPfcInfo.hh   XrdPfcInfo.cc
  XrdPfcPrint.hh  XrdPfcPrint.cc
  XrdPfcTypes.hh
)

target_link_libraries(xrdpfc_print XrdServer XrdCl XrdUtils)

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf lib${XrdPfc}.so lib${XrdFileCache}.so
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} )" )

install(
  FILES
    XrdPfcPurgePin.hh
    XrdPfcDirStateBase.hh
    XrdPfcDirStatePurgeshot.hh
    XrdPfcStats.hh
    XrdPfcPathParseTools.hh
    XrdPfc.hh
    XrdPfcFile.hh
    XrdPfcTypes.hh
    XrdPfcInfo.hh
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/XrdPfc
)

install(TARGETS xrdpfc_print RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS ${XrdPfc} ${XrdBlacklistDecision} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
