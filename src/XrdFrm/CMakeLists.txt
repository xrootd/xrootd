add_library(XrdFrm STATIC
  XrdFrmConfig.cc     XrdFrmConfig.hh
  XrdFrmFiles.cc      XrdFrmFiles.hh
  XrdFrmMonitor.cc    XrdFrmMonitor.hh
  XrdFrmTSort.cc      XrdFrmTSort.hh
  XrdFrmCns.cc        XrdFrmCns.hh

  XrdFrmMigrate.cc    XrdFrmMigrate.hh
  XrdFrmReqBoss.cc    XrdFrmReqBoss.hh
  XrdFrmTransfer.cc   XrdFrmTransfer.hh
  XrdFrmXfrAgent.cc   XrdFrmXfrAgent.hh
  XrdFrmXfrDaemon.cc  XrdFrmXfrDaemon.hh
                      XrdFrmXfrJob.hh
  XrdFrmXfrQueue.cc   XrdFrmXfrQueue.hh
)

add_executable(frm_admin
  XrdFrmAdminAudit.cc
  XrdFrmAdmin.cc       XrdFrmAdmin.hh
  XrdFrmAdminFiles.cc
  XrdFrmAdminFind.cc
  XrdFrmAdminMain.cc
  XrdFrmAdminQuery.cc
  XrdFrmAdminUnlink.cc
)

target_link_libraries(frm_admin
  XrdFrm
  XrdServer
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${READLINE_LIBRARY}
  ${NCURSES_LIBRARY}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY}
)

if(READLINE_FOUND)
  target_include_directories(frm_admin PRIVATE ${READLINE_INCLUDE_DIR})
endif()

add_executable(frm_purged
  XrdFrmPurge.cc     XrdFrmPurge.hh
  XrdFrmPurgMain.cc
)

target_link_libraries(frm_purged
  XrdFrm
  XrdServer
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY}
)

add_executable(frm_xfrd XrdFrmXfrMain.cc)

target_link_libraries(frm_xfrd
  XrdFrm
  XrdServer
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY}
)

add_executable(frm_xfragent XrdFrmXfrMain.cc)

target_link_libraries(frm_xfragent
  XrdFrm
  XrdServer
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY}
)

install(
  TARGETS
    frm_admin
    frm_purged
    frm_xfragent
    frm_xfrd
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
