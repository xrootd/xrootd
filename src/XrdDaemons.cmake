

#-------------------------------------------------------------------------------
# xrootd
#-------------------------------------------------------------------------------
add_executable(
  xrootd
  Xrd/XrdConfig.cc          Xrd/XrdConfig.hh
  Xrd/XrdProtLoad.cc            Xrd/XrdProtLoad.hh
  Xrd/XrdStats.cc               Xrd/XrdStats.hh
  Xrd/XrdMain.cc )

target_link_libraries(
  xrootd
  XrdServer
  XrdUtils
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

#-------------------------------------------------------------------------------
# cmsd
#-------------------------------------------------------------------------------
add_executable(
  cmsd
  Xrd/XrdConfig.cc                Xrd/XrdConfig.hh
  Xrd/XrdProtLoad.cc              Xrd/XrdProtLoad.hh
  Xrd/XrdStats.cc                 Xrd/XrdStats.hh
  Xrd/XrdMain.cc
  XrdCms/XrdCmsAdmin.cc           XrdCms/XrdCmsAdmin.hh
  XrdCms/XrdCmsBaseFS.cc          XrdCms/XrdCmsBaseFS.hh
  XrdCms/XrdCmsCache.cc           XrdCms/XrdCmsCache.hh
  XrdCms/XrdCmsCluster.cc         XrdCms/XrdCmsCluster.hh
  XrdCms/XrdCmsClustID.cc         XrdCms/XrdCmsClustID.hh
  XrdCms/XrdCmsConfig.cc          XrdCms/XrdCmsConfig.hh
  XrdCms/XrdCmsJob.cc             XrdCms/XrdCmsJob.hh
  XrdCms/XrdCmsKey.cc             XrdCms/XrdCmsKey.hh
  XrdCms/XrdCmsManager.cc         XrdCms/XrdCmsManager.hh
  XrdCms/XrdCmsManList.cc         XrdCms/XrdCmsManList.hh
  XrdCms/XrdCmsManTree.cc         XrdCms/XrdCmsManTree.hh
  XrdCms/XrdCmsMeter.cc           XrdCms/XrdCmsMeter.hh
  XrdCms/XrdCmsNash.cc            XrdCms/XrdCmsNash.hh
  XrdCms/XrdCmsNode.cc            XrdCms/XrdCmsNode.hh
  XrdCms/XrdCmsPList.cc           XrdCms/XrdCmsPList.hh
  XrdCms/XrdCmsPrepare.cc         XrdCms/XrdCmsPrepare.hh
  XrdCms/XrdCmsPrepArgs.cc        XrdCms/XrdCmsPrepArgs.hh
  XrdCms/XrdCmsProtocol.cc        XrdCms/XrdCmsProtocol.hh
  XrdCms/XrdCmsRouting.cc         XrdCms/XrdCmsRouting.hh
  XrdCms/XrdCmsRRQ.cc             XrdCms/XrdCmsRRQ.hh
                                  XrdCms/XrdCmsSelect.hh
  XrdCms/XrdCmsState.cc           XrdCms/XrdCmsState.hh
  XrdCms/XrdCmsSupervisor.cc      XrdCms/XrdCmsSupervisor.hh
                                  XrdCms/XrdCmsTrace.hh )
target_link_libraries(
  cmsd
  XrdServer
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${ATOMIC_LIBRARY}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

if( CMAKE_COMPILER_IS_GNUCXX )
  target_compile_options(cmsd INTERFACE -msse4.2)
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS xrootd cmsd
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
