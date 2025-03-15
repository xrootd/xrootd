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

#-------------------------------------------------------------------------------
# The XrdCmsRedirLocal module
#-------------------------------------------------------------------------------
set(XrdCmsRedirLocal XrdCmsRedirectLocal-${PLUGIN_VERSION})

add_library(${XrdCmsRedirLocal} MODULE XrdCmsRedirLocal.cc XrdCmsRedirLocal.hh)

target_link_libraries(${XrdCmsRedirLocal} PRIVATE XrdServer XrdUtils)

install(TARGETS ${XrdCmsRedirLocal} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
