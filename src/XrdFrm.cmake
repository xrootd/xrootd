
include( XRootDCommon )

#-------------------------------------------------------------------------------
# The XrdFrm library
#-------------------------------------------------------------------------------
add_library(
  XrdFrm
  STATIC
  XrdFrm/XrdFrmConfig.cc        XrdFrm/XrdFrmConfig.hh
  XrdFrm/XrdFrmFiles.cc         XrdFrm/XrdFrmFiles.hh
  XrdFrm/XrdFrmMonitor.cc       XrdFrm/XrdFrmMonitor.hh
  XrdFrm/XrdFrmTSort.cc         XrdFrm/XrdFrmTSort.hh
  XrdFrm/XrdFrmCns.cc           XrdFrm/XrdFrmCns.hh

  XrdFrm/XrdFrmMigrate.cc       XrdFrm/XrdFrmMigrate.hh
  XrdFrm/XrdFrmReqBoss.cc       XrdFrm/XrdFrmReqBoss.hh
  XrdFrm/XrdFrmTransfer.cc      XrdFrm/XrdFrmTransfer.hh
  XrdFrm/XrdFrmXfrAgent.cc      XrdFrm/XrdFrmXfrAgent.hh
  XrdFrm/XrdFrmXfrDaemon.cc     XrdFrm/XrdFrmXfrDaemon.hh
                                XrdFrm/XrdFrmXfrJob.hh
  XrdFrm/XrdFrmXfrQueue.cc      XrdFrm/XrdFrmXfrQueue.hh
)

#-------------------------------------------------------------------------------
# frm_admin
#-------------------------------------------------------------------------------
add_executable(
  frm_admin
  XrdFrm/XrdFrmAdminAudit.cc
  XrdFrm/XrdFrmAdmin.cc            XrdFrm/XrdFrmAdmin.hh
  XrdFrm/XrdFrmAdminConvert.cc
  XrdFrm/XrdFrmAdminFiles.cc
  XrdFrm/XrdFrmAdminFind.cc
  XrdFrm/XrdFrmAdminMain.cc
  XrdFrm/XrdFrmAdminQuery.cc
  XrdFrm/XrdFrmAdminUnlink.cc )

target_link_libraries(
  frm_admin
  XrdFrm
  XrdServer
  XrdUtils
  pthread
  ${READLINE_LIBRARY}
  ${NCURSES_LIBRARY}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

#-------------------------------------------------------------------------------
# frm_purged
#-------------------------------------------------------------------------------
add_executable(
  frm_purged
  XrdFrm/XrdFrmPurge.cc             XrdFrm/XrdFrmPurge.hh
  XrdFrm/XrdFrmPurgMain.cc )

target_link_libraries(
  frm_purged
  XrdFrm
  XrdServer
  XrdUtils
  pthread
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

#-------------------------------------------------------------------------------
# frm_xfrd
#-------------------------------------------------------------------------------
add_executable(
  frm_xfrd
  XrdFrm/XrdFrmXfrMain.cc )

target_link_libraries(
  frm_xfrd
  XrdFrm
  XrdServer
  XrdUtils
  pthread
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

#-------------------------------------------------------------------------------
# frm_xfragent
#-------------------------------------------------------------------------------
add_executable(
  frm_xfragent
  XrdFrm/XrdFrmXfrMain.cc )

target_link_libraries(
  frm_xfragent
  XrdFrm
  XrdServer
  XrdUtils
  pthread
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS frm_admin frm_purged frm_xfrd frm_xfragent
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/frm_admin.8
  ${PROJECT_SOURCE_DIR}/docs/man/frm_purged.8
  ${PROJECT_SOURCE_DIR}/docs/man/frm_xfrd.8
  ${PROJECT_SOURCE_DIR}/docs/man/frm_xfragent.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 )
