
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_UTILS_VERSION   2.0.0 )
set( XRD_UTILS_SOVERSION 2 )
set( XRD_ZCRC32_VERSION   2.0.0 )
set( XRD_ZCRC32_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdSys library
#-------------------------------------------------------------------------------
add_library(
  XrdUtils
  SHARED

  #-----------------------------------------------------------------------------
  # XrdSys
  #-----------------------------------------------------------------------------
  XrdSys/XrdSysDNS.cc           XrdSys/XrdSysDNS.hh
  XrdSys/XrdSysDir.cc           XrdSys/XrdSysDir.hh
                                XrdSys/XrdSysFD.hh
  XrdSys/XrdSysPlugin.cc        XrdSys/XrdSysPlugin.hh
  XrdSys/XrdSysPriv.cc          XrdSys/XrdSysPriv.hh
  XrdSys/XrdSysPlatform.cc      XrdSys/XrdSysPlatform.hh
  XrdSys/XrdSysPthread.cc       XrdSys/XrdSysPthread.hh
                                XrdSys/XrdSysSemWait.hh
  XrdSys/XrdSysTimer.cc         XrdSys/XrdSysTimer.hh
  XrdSys/XrdSysUtils.cc         XrdSys/XrdSysUtils.hh
  XrdSys/XrdSysXSLock.cc        XrdSys/XrdSysXSLock.hh
  XrdSys/XrdSysFAttr.cc         XrdSys/XrdSysFAttr.hh
                                XrdSys/XrdSysFAttrBsd.icc
                                XrdSys/XrdSysFAttrLnx.icc
                                XrdSys/XrdSysFAttrMac.icc
                                XrdSys/XrdSysFAttrSun.icc
  XrdSys/XrdSysIOEvents.cc      XrdSys/XrdSysIOEvents.hh
                                XrdSys/XrdSysIOEventsPollE.icc
                                XrdSys/XrdSysIOEventsPollKQ.icc
                                XrdSys/XrdSysIOEventsPollPoll.icc
                                XrdSys/XrdSysIOEventsPollPort.icc
                                XrdSys/XrdSysAtomics.hh
                                XrdSys/XrdSysHeaders.hh
  XrdSys/XrdSysError.cc         XrdSys/XrdSysError.hh
  XrdSys/XrdSysLogger.cc        XrdSys/XrdSysLogger.hh
                                XrdSys/XrdSysLinuxSemaphore.hh
  XrdSys/XrdSysXAttr.cc         XrdSys/XrdSysXAttr.hh

  #-----------------------------------------------------------------------------
  # XrdOuc
  #-----------------------------------------------------------------------------
  XrdOuc/XrdOuca2x.cc           XrdOuc/XrdOuca2x.hh
  XrdOuc/XrdOucArgs.cc          XrdOuc/XrdOucArgs.hh
  XrdOuc/XrdOucBuffer.cc        XrdOuc/XrdOucBuffer.hh
                                XrdOuc/XrdOucCache.hh
  XrdOuc/XrdOucCacheData.cc     XrdOuc/XrdOucCacheData.hh
  XrdOuc/XrdOucCacheDram.cc     XrdOuc/XrdOucCacheDram.hh
  XrdOuc/XrdOucCacheReal.cc     XrdOuc/XrdOucCacheReal.hh
                                XrdOuc/XrdOucCacheSlot.hh
  XrdOuc/XrdOucCallBack.cc      XrdOuc/XrdOucCallBack.hh
  XrdOuc/XrdOucCRC.cc           XrdOuc/XrdOucCRC.hh
  XrdOuc/XrdOucEnv.cc           XrdOuc/XrdOucEnv.hh
                                XrdOuc/XrdOucHash.hh
                                XrdOuc/XrdOucHash.icc
  XrdOuc/XrdOucERoute.cc        XrdOuc/XrdOucERoute.hh
                                XrdOuc/XrdOucErrInfo.hh
  XrdOuc/XrdOucExport.cc        XrdOuc/XrdOucExport.hh
  XrdOuc/XrdOucGMap.cc          XrdOuc/XrdOucGMap.hh
  XrdOuc/XrdOucHashVal.cc
  XrdOuc/XrdOucMsubs.cc         XrdOuc/XrdOucMsubs.hh
  XrdOuc/XrdOucName2Name.cc     XrdOuc/XrdOucName2Name.hh
  XrdOuc/XrdOucN2NLoader.cc     XrdOuc/XrdOucN2NLoader.hh
  XrdOuc/XrdOucNList.cc         XrdOuc/XrdOucNList.hh
  XrdOuc/XrdOucNSWalk.cc        XrdOuc/XrdOucNSWalk.hh
  XrdOuc/XrdOucPinLoader.cc     XrdOuc/XrdOucPinLoader.hh
  XrdOuc/XrdOucPinPath.cc       XrdOuc/XrdOucPinPath.hh
  XrdOuc/XrdOucPreload.cc       XrdOuc/XrdOucPreload.hh
  XrdOuc/XrdOucProg.cc          XrdOuc/XrdOucProg.hh
  XrdOuc/XrdOucPup.cc           XrdOuc/XrdOucPup.hh
  XrdOuc/XrdOucReqID.cc         XrdOuc/XrdOucReqID.hh
  XrdOuc/XrdOucSid.cc           XrdOuc/XrdOucSid.hh
  XrdOuc/XrdOucSiteName.cc      XrdOuc/XrdOucSiteName.hh
  XrdOuc/XrdOucStream.cc        XrdOuc/XrdOucStream.hh
  XrdOuc/XrdOucString.cc        XrdOuc/XrdOucString.hh
  XrdOuc/XrdOucSxeq.cc          XrdOuc/XrdOucSxeq.hh
  XrdOuc/XrdOucTokenizer.cc     XrdOuc/XrdOucTokenizer.hh
  XrdOuc/XrdOucTPC.cc           XrdOuc/XrdOucTPC.hh
  XrdOuc/XrdOucTrace.cc         XrdOuc/XrdOucTrace.hh
  XrdOuc/XrdOucUtils.cc         XrdOuc/XrdOucUtils.hh
  XrdOuc/XrdOucVerName.cc       XrdOuc/XrdOucVerName.hh
                                XrdOuc/XrdOucChain.hh
                                XrdOuc/XrdOucDLlist.hh
                                XrdOuc/XrdOucIOVec.hh
                                XrdOuc/XrdOucLock.hh
                                XrdOuc/XrdOucPList.hh
                                XrdOuc/XrdOucRash.hh
                                XrdOuc/XrdOucRash.icc
                                XrdOuc/XrdOucTable.hh
                                XrdOuc/XrdOucTList.hh
                                XrdOuc/XrdOucXAttr.hh
                                XrdOuc/XrdOucEnum.hh

  #-----------------------------------------------------------------------------
  # XrdNet
  #-----------------------------------------------------------------------------
  XrdNet/XrdNet.cc              XrdNet/XrdNet.hh
                                XrdNet/XrdNetOpts.hh
                                XrdNet/XrdNetPeer.hh
                                XrdNet/XrdNetSockAddr.hh
  XrdNet/XrdNetAddr.cc          XrdNet/XrdNetAddr.hh
  XrdNet/XrdNetAddrInfo.cc      XrdNet/XrdNetAddrInfo.hh
  XrdNet/XrdNetBuffer.cc        XrdNet/XrdNetBuffer.hh
  XrdNet/XrdNetCache.cc         XrdNet/XrdNetCache.hh
  XrdNet/XrdNetCmsNotify.cc     XrdNet/XrdNetCmsNotify.hh
  XrdNet/XrdNetConnect.cc       XrdNet/XrdNetConnect.hh
  XrdNet/XrdNetIF.cc            XrdNet/XrdNetIF.hh
  XrdNet/XrdNetMsg.cc           XrdNet/XrdNetMsg.hh
  XrdNet/XrdNetSecurity.cc      XrdNet/XrdNetSecurity.hh
  XrdNet/XrdNetSocket.cc        XrdNet/XrdNetSocket.hh
  XrdNet/XrdNetUtils.cc         XrdNet/XrdNetUtils.hh

  #-----------------------------------------------------------------------------
  # XrdSut
  #-----------------------------------------------------------------------------
  XrdSut/XrdSutAux.cc           XrdSut/XrdSutAux.hh
  XrdSut/XrdSutCache.cc         XrdSut/XrdSutCache.hh
  XrdSut/XrdSutBucket.cc        XrdSut/XrdSutBucket.hh
  XrdSut/XrdSutBuckList.cc      XrdSut/XrdSutBuckList.hh
  XrdSut/XrdSutBuffer.cc        XrdSut/XrdSutBuffer.hh
  XrdSut/XrdSutPFile.cc         XrdSut/XrdSutPFile.hh
  XrdSut/XrdSutPFEntry.cc       XrdSut/XrdSutPFEntry.hh
  XrdSut/XrdSutRndm.cc          XrdSut/XrdSutRndm.hh
  XrdSut/XrdSutTrace.hh

  #-----------------------------------------------------------------------------
  # Xrd
  #-----------------------------------------------------------------------------
  Xrd/XrdBuffer.cc              Xrd/XrdBuffer.hh
  Xrd/XrdInet.cc                Xrd/XrdInet.hh
  Xrd/XrdInfo.cc                Xrd/XrdInfo.hh
  Xrd/XrdJob.hh
  Xrd/XrdLink.cc                Xrd/XrdLink.hh
  Xrd/XrdLinkMatch.cc           Xrd/XrdLinkMatch.hh
  Xrd/XrdPoll.cc                Xrd/XrdPoll.hh
                                Xrd/XrdPollDev.hh
                                Xrd/XrdPollDev.icc
                                Xrd/XrdPollE.hh
                                Xrd/XrdPollE.icc
                                Xrd/XrdPollPoll.hh
                                Xrd/XrdPollPoll.icc
  Xrd/XrdProtocol.cc            Xrd/XrdProtocol.hh
  Xrd/XrdScheduler.cc           Xrd/XrdScheduler.hh
                                Xrd/XrdTrace.hh

  #-----------------------------------------------------------------------------
  # XrdCks
  #-----------------------------------------------------------------------------
  XrdCks/XrdCksCalccrc32.cc        XrdCks/XrdCksCalccrc32.hh
  XrdCks/XrdCksCalcmd5.cc          XrdCks/XrdCksCalcmd5.hh
  XrdCks/XrdCksConfig.cc           XrdCks/XrdCksConfig.hh
  XrdCks/XrdCksLoader.cc           XrdCks/XrdCksLoader.hh
  XrdCks/XrdCksManager.cc          XrdCks/XrdCksManager.hh
  XrdCks/XrdCksManOss.cc           XrdCks/XrdCksManOss.hh
                                   XrdCks/XrdCksCalcadler32.hh
                                   XrdCks/XrdCksCalc.hh
                                   XrdCks/XrdCksData.hh
                                   XrdCks/XrdCks.hh
                                   XrdCks/XrdCksXAttr.hh

  #-----------------------------------------------------------------------------
  # XrdSec
  #-----------------------------------------------------------------------------
  XrdSec/XrdSecLoadSecurity.cc     XrdSec/XrdSecLoadSecurity.hh
  XrdSecsss/XrdSecsssID.cc         XrdSecsss/XrdSecsssID.hh
  XrdSecsss/XrdSecsssKT.cc         XrdSecsss/XrdSecsssKT.hh

)

target_link_libraries(
  XrdUtils
  pthread
  dl
  ${SOCKET_LIBRARY}
  ${SENDFILE_LIBRARY}
  ${EXTRA_LIBS} )

set_target_properties(
  XrdUtils
  PROPERTIES
  VERSION   ${XRD_UTILS_VERSION}
  SOVERSION ${XRD_UTILS_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdUtils
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
