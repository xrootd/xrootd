#-----------------------------------------------------------------------------
# XrdCms - client for clustering
#-----------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdCmsBlackList.cc     XrdCmsBlackList.hh
    XrdCmsClientConfig.cc  XrdCmsClientConfig.hh
    XrdCmsClientMan.cc     XrdCmsClientMan.hh
    XrdCmsClientMsg.cc     XrdCmsClientMsg.hh
    XrdCmsClient.cc        XrdCmsClient.hh
    XrdCmsFinder.cc        XrdCmsFinder.hh
    XrdCmsLogin.cc         XrdCmsLogin.hh
    XrdCmsParser.cc        XrdCmsParser.hh
                           XrdCmsPerfMon.hh
    XrdCmsResp.cc          XrdCmsResp.hh
    XrdCmsRRData.cc        XrdCmsRRData.hh
    XrdCmsRTable.cc        XrdCmsRTable.hh
    XrdCmsSecurity.cc      XrdCmsSecurity.hh
    XrdCmsTalk.cc          XrdCmsTalk.hh
                           XrdCmsTypes.hh
    XrdCmsUtils.cc         XrdCmsUtils.hh
                           XrdCmsVnId.hh
)

add_executable(cmsd
  ../Xrd/XrdConfig.cc    ../Xrd/XrdConfig.hh
  ../Xrd/XrdProtLoad.cc  ../Xrd/XrdProtLoad.hh
  ../Xrd/XrdStats.cc     ../Xrd/XrdStats.hh
  ../Xrd/XrdMain.cc

  XrdCmsAdmin.cc       XrdCmsAdmin.hh
  XrdCmsBaseFS.cc      XrdCmsBaseFS.hh
  XrdCmsCache.cc       XrdCmsCache.hh
  XrdCmsCluster.cc     XrdCmsCluster.hh
  XrdCmsClustID.cc     XrdCmsClustID.hh
  XrdCmsConfig.cc      XrdCmsConfig.hh
  XrdCmsJob.cc         XrdCmsJob.hh
  XrdCmsKey.cc         XrdCmsKey.hh
  XrdCmsManager.cc     XrdCmsManager.hh
  XrdCmsManList.cc     XrdCmsManList.hh
  XrdCmsManTree.cc     XrdCmsManTree.hh
  XrdCmsMeter.cc       XrdCmsMeter.hh
  XrdCmsNash.cc        XrdCmsNash.hh
  XrdCmsNode.cc        XrdCmsNode.hh
  XrdCmsPList.cc       XrdCmsPList.hh
  XrdCmsPrepare.cc     XrdCmsPrepare.hh
  XrdCmsPrepArgs.cc    XrdCmsPrepArgs.hh
  XrdCmsProtocol.cc    XrdCmsProtocol.hh
  XrdCmsRouting.cc     XrdCmsRouting.hh
  XrdCmsRRQ.cc         XrdCmsRRQ.hh
                       XrdCmsSelect.hh
  XrdCmsState.cc       XrdCmsState.hh
  XrdCmsSupervisor.cc  XrdCmsSupervisor.hh
                       XrdCmsTrace.hh
)

if(CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options(cmsd INTERFACE -msse4.2)
endif()

target_link_libraries(cmsd
  XrdServer
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${ATOMIC_LIBRARY}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY}
)

install(TARGETS cmsd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

#-------------------------------------------------------------------------------
# The XrdCmsRedirLocal module
#-------------------------------------------------------------------------------
set(XrdCmsRedirLocal XrdCmsRedirectLocal-${PLUGIN_VERSION})

add_library(${XrdCmsRedirLocal} MODULE XrdCmsRedirLocal.cc XrdCmsRedirLocal.hh)

target_link_libraries(${XrdCmsRedirLocal} PRIVATE XrdServer XrdUtils)

install(TARGETS ${XrdCmsRedirLocal} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
