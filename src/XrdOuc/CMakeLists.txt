target_sources(XrdUtils
  PRIVATE
    XrdOuca2x.cc         XrdOuca2x.hh
    XrdOucArgs.cc        XrdOucArgs.hh
    XrdOucBackTrace.cc   XrdOucBackTrace.hh
    XrdOucBuffer.cc      XrdOucBuffer.hh
    XrdOucCache.cc       XrdOucCache.hh
                         XrdOucCacheStats.hh
    XrdOucCallBack.cc    XrdOucCallBack.hh
                         XrdOucChkPnt.hh
                         XrdOucCloneSeg.hh
    XrdOucCRC.cc         XrdOucCRC.hh
    XrdOucCRC32C.cc      XrdOucCRC32C.hh
    XrdOucECMsg.cc       XrdOucECMsg.hh
    XrdOucEnv.cc         XrdOucEnv.hh
    XrdOucERoute.cc      XrdOucERoute.hh
                         XrdOucErrInfo.hh
    XrdOucExport.cc      XrdOucExport.hh
    XrdOucFileInfo.cc    XrdOucFileInfo.hh
    XrdOucGatherConf.cc  XrdOucGatherConf.hh
    XrdOucGMap.cc        XrdOucGMap.hh
                         XrdOucHash.hh
                         XrdOucHash.icc
    XrdOucHashVal.cc
                         XrdOucJson.hh
    XrdOucLogging.cc     XrdOucLogging.hh
                         XrdOucMapP2X.hh
    XrdOucMsubs.cc       XrdOucMsubs.hh
    XrdOucName2Name.cc   XrdOucName2Name.hh
    XrdOucN2NLoader.cc   XrdOucN2NLoader.hh
    XrdOucNList.cc       XrdOucNList.hh
    XrdOucNSWalk.cc      XrdOucNSWalk.hh
    XrdOucPgrwUtils.cc   XrdOucPgrwUtils.hh
                         XrdOucPinKing.hh
    XrdOucPinLoader.cc   XrdOucPinLoader.hh
                         XrdOucPinObject.hh
    XrdOucPinPath.cc     XrdOucPinPath.hh
    XrdOucPreload.cc     XrdOucPreload.hh
    XrdOucProg.cc        XrdOucProg.hh
    XrdOucPsx.cc         XrdOucPsx.hh
    XrdOucPup.cc         XrdOucPup.hh
    XrdOucReqID.cc       XrdOucReqID.hh
    XrdOucSHA3.cc        XrdOucSHA3.hh
    XrdOucSid.cc         XrdOucSid.hh
    XrdOucSiteName.cc    XrdOucSiteName.hh
    XrdOucStream.cc      XrdOucStream.hh
    XrdOucString.cc      XrdOucString.hh
    XrdOucSxeq.cc        XrdOucSxeq.hh
    XrdOucTokenizer.cc   XrdOucTokenizer.hh
    XrdOucTPC.cc         XrdOucTPC.hh
    XrdOucTrace.cc       XrdOucTrace.hh
    XrdOucUri.cc         XrdOucUri.hh
    XrdOucUtils.cc       XrdOucUtils.hh
    XrdOucVerName.cc     XrdOucVerName.hh
                         XrdOucChain.hh
                         XrdOucDLlist.hh
                         XrdOucIOVec.hh
                         XrdOucLock.hh
                         XrdOucPList.hh
                         XrdOucRash.hh
                         XrdOucRash.icc
                         XrdOucTable.hh
                         XrdOucTList.hh
                         XrdOucXAttr.hh
                         XrdOucEnum.hh
)

#-------------------------------------------------------------------------------
# N2No2p plugin library
#-------------------------------------------------------------------------------
set(XrdN2No2p XrdN2No2p-${PLUGIN_VERSION})
add_library(${XrdN2No2p} MODULE XrdOucN2No2p.cc)
target_link_libraries(${XrdN2No2p} PRIVATE XrdUtils)

install(TARGETS ${XrdN2No2p} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
