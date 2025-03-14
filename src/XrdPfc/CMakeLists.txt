set(XrdBlacklistDecision XrdBlacklistDecision-${PLUGIN_VERSION})

add_library(${XrdBlacklistDecision} MODULE XrdPfcBlacklistDecision.cc)
target_link_libraries(${XrdBlacklistDecision} PRIVATE XrdUtils)

set(XrdPfc XrdPfc-${PLUGIN_VERSION})
set(XrdFileCache XrdFileCache-${PLUGIN_VERSION})
set(XrdPfcPurgeQuota XrdPfcPurgeQuota-${PLUGIN_VERSION})

add_library(${XrdPfc} MODULE
  XrdPfc.cc                 XrdPfc.hh
  XrdPfcCommand.cc
  XrdPfcConfiguration.cc
                            XrdPfcDecision.hh
  XrdPfcDirState.cc         XrdPfcDirState.hh
                            XrdPfcDirStateBase.hh
                            XrdPfcDirStatePurgeshot.hh
  XrdPfcDirStateSnapshot.cc XrdPfcDirStateSnapshot.hh
  XrdPfcFPurgeState.cc      XrdPfcFPurgeState.hh
  XrdPfcFSctl.cc            XrdPfcFSctl.hh
  XrdPfcFile.cc             XrdPfcFile.hh
  XrdPfcFsTraversal.cc      XrdPfcFsTraversal.hh
  XrdPfcIO.cc               XrdPfcIO.hh
  XrdPfcIOFile.cc           XrdPfcIOFile.hh
  XrdPfcIOFileBlock.cc      XrdPfcIOFileBlock.hh
  XrdPfcInfo.cc             XrdPfcInfo.hh
                            XrdPfcPathParseTools.hh
  XrdPfcPurge.cc
                            XrdPfcPurgePin.hh
  XrdPfcResourceMonitor.cc  XrdPfcResourceMonitor.hh
                            XrdPfcStats.hh
                            XrdPfcTypes.hh
)

install(
  FILES
    XrdPfc.hh
    XrdPfcDirStateBase.hh
    XrdPfcDirStatePurgeshot.hh
    XrdPfcFile.hh
    XrdPfcInfo.hh
    XrdPfcPathParseTools.hh
    XrdPfcPurgePin.hh
    XrdPfcStats.hh
    XrdPfcTypes.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdPfc
)

target_link_libraries(${XrdPfc} PRIVATE XrdCl XrdUtils XrdServer)

add_library(${XrdPfcPurgeQuota} MODULE XrdPfcPurgeQuota.cc)
target_link_libraries(${XrdPfcPurgeQuota} PRIVATE XrdUtils)

install(
  TARGETS
    ${XrdPfc}
    ${XrdPfcPurgeQuota}
    ${XrdBlacklistDecision}
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(CODE "
  execute_process(COMMAND ln -sf lib${XrdPfc}.so lib${XrdFileCache}.so
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
  )
")

add_executable(xrdpfc_print
  XrdPfcInfo.cc   XrdPfcInfo.hh
  XrdPfcPrint.cc  XrdPfcPrint.hh
                  XrdPfcTypes.hh
)

target_link_libraries(xrdpfc_print XrdServer XrdCl XrdUtils)

install(TARGETS xrdpfc_print RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
