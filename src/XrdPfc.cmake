
#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_FILECACHE  XrdPfc-${PLUGIN_VERSION} )
set( LIB_XRD_FILECACHE_LEGACY XrdFileCache-${PLUGIN_VERSION} )
set( LIB_XRD_BLACKLIST  XrdBlacklistDecision-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# The XrdPfc library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_FILECACHE}
  MODULE
  XrdPfc/XrdPfcTypes.hh
  XrdPfc/XrdPfc.cc              XrdPfc/XrdPfc.hh
  XrdPfc/XrdPfcConfiguration.cc
  XrdPfc/XrdPfcPurge.cc
  XrdPfc/XrdPfcCommand.cc
  XrdPfc/XrdPfcFile.cc          XrdPfc/XrdPfcFile.hh
  XrdPfc/XrdPfcFSctl.cc         XrdPfc/XrdPfcFSctl.hh
  XrdPfc/XrdPfcStats.hh
  XrdPfc/XrdPfcInfo.cc          XrdPfc/XrdPfcInfo.hh
  XrdPfc/XrdPfcIO.cc            XrdPfc/XrdPfcIO.hh
  XrdPfc/XrdPfcIOFile.cc        XrdPfc/XrdPfcIOFile.hh
  XrdPfc/XrdPfcIOFileBlock.cc   XrdPfc/XrdPfcIOFileBlock.hh
  XrdPfc/XrdPfcDecision.hh)

target_link_libraries(
  ${LIB_XRD_FILECACHE}
  PRIVATE
# XrdPosix
  XrdCl
  XrdUtils
  XrdServer
  ${CMAKE_THREAD_LIBS_INIT} )

#-------------------------------------------------------------------------------
# The XrdBlacklistDecision library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_BLACKLIST}
  MODULE
  XrdPfc/XrdPfcBlacklistDecision.cc) 

target_link_libraries(
  ${LIB_XRD_BLACKLIST}
  PRIVATE
  XrdUtils
  )

#-------------------------------------------------------------------------------
# xrdpfc_print
#-------------------------------------------------------------------------------
add_executable(
  xrdpfc_print
  XrdPfc/XrdPfcPrint.hh  XrdPfc/XrdPfcPrint.cc
  XrdPfc/XrdPfcTypes.hh
  XrdPfc/XrdPfcInfo.hh   XrdPfc/XrdPfcInfo.cc)

target_link_libraries(
  xrdpfc_print
  XrdServer
  XrdCl
  XrdUtils )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_FILECACHE}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf lib${LIB_XRD_FILECACHE}.so lib${LIB_XRD_FILECACHE_LEGACY}.so
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} )" )

install(
  TARGETS ${LIB_XRD_BLACKLIST}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  TARGETS xrdpfc_print
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
