set(XrdSsi     XrdSsi-${PLUGIN_VERSION})
set(XrdSsiLog  XrdSsiLog-${PLUGIN_VERSION})

add_library(XrdSsiLib SHARED
  XrdSsiAlert.cc         XrdSsiAlert.hh
  XrdSsiAtomics.cc       XrdSsiAtomics.hh
                         XrdSsiBVec.hh
  XrdSsiClient.cc
                         XrdSsiCluster.hh
  XrdSsiCms.cc           XrdSsiCms.hh
  XrdSsiErrInfo.cc       XrdSsiErrInfo.hh
  XrdSsiEvent.cc         XrdSsiEvent.hh
  XrdSsiFileResource.cc  XrdSsiFileResource.hh
  XrdSsiLogger.cc        XrdSsiLogger.hh
                         XrdSsiProvider.hh
                         XrdSsiRRAgent.hh
                         XrdSsiRRInfo.hh
                         XrdSsiRRTable.hh
  XrdSsiRequest.cc       XrdSsiRequest.hh
  XrdSsiResponder.cc     XrdSsiResponder.hh
                         XrdSsiResource.hh
  XrdSsiScale.cc         XrdSsiScale.hh
  XrdSsiServReal.cc      XrdSsiServReal.hh
  XrdSsiService.cc       XrdSsiService.hh
  XrdSsiSessReal.cc      XrdSsiSessReal.hh
  XrdSsiStats.cc         XrdSsiStats.hh
                         XrdSsiStream.hh
  XrdSsiTaskReal.cc      XrdSsiTaskReal.hh
                         XrdSsiTrace.hh
  XrdSsiUtils.cc         XrdSsiUtils.hh
)

target_link_libraries(XrdSsiLib PRIVATE XrdCl XrdUtils)
set_target_properties(XrdSsiLib PROPERTIES VERSION 2.0.0 SOVERSION 2)

add_library(XrdSsiShMap SHARED
  XrdSsiShMam.cc   XrdSsiShMam.hh
  XrdSsiShMap.icc  XrdSsiShMap.hh
  XrdSsiShMat.cc   XrdSsiShMat.hh
)

target_link_libraries(XrdSsiShMap PRIVATE XrdUtils ZLIB::ZLIB)
set_target_properties(XrdSsiShMap PROPERTIES VERSION 2.0.0 SOVERSION 2)

add_library(${XrdSsi} MODULE
  XrdSsiDir.cc       XrdSsiDir.hh
  XrdSsiFile.cc      XrdSsiFile.hh
  XrdSsiFileReq.cc   XrdSsiFileReq.hh
  XrdSsiFileSess.cc  XrdSsiFileSess.hh
  XrdSsiSfs.cc       XrdSsiSfs.hh
  XrdSsiSfsConfig.cc XrdSsiSfsConfig.hh
  XrdSsiStat.cc
)

target_link_libraries(${XrdSsi} PRIVATE XrdSsiLib XrdUtils XrdServer)

add_library(${XrdSsiLog} MODULE XrdSsiLogging.cc)
target_link_libraries(${XrdSsiLog} PRIVATE XrdSsiLib XrdUtils XrdServer)

install(
  TARGETS
    ${XrdSsi}
    ${XrdSsiLog}
    XrdSsiLib
    XrdSsiShMap
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
