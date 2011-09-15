
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_UTILS_VERSION   0.0.1 )
set( XRD_UTILS_SOVERSION 0 )
set( XRD_PROTOCOL_LOADER_VERSION   0.0.1 )
set( XRD_PROTOCOL_LOADER_SOVERSION 0 )

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
  XrdSys/XrdSysPlugin.cc        XrdSys/XrdSysPlugin.hh
  XrdSys/XrdSysPriv.cc          XrdSys/XrdSysPriv.hh
  XrdSys/XrdSysPlatform.cc      XrdSys/XrdSysPlatform.hh
  XrdSys/XrdSysPthread.cc       XrdSys/XrdSysPthread.hh
                                XrdSys/XrdSysSemWait.hh
  XrdSys/XrdSysTimer.cc         XrdSys/XrdSysTimer.hh
  XrdSys/XrdSysXSLock.cc        XrdSys/XrdSysXSLock.hh
  XrdSys/XrdSysFAttr.cc         XrdSys/XrdSysFAttr.hh
                                XrdSys/XrdSysFAttrBsd.icc
                                XrdSys/XrdSysFAttrLnx.icc
                                XrdSys/XrdSysFAttrMac.icc
                                XrdSys/XrdSysFAttrSun.icc
                                XrdSys/XrdSysAtomics.hh
                                XrdSys/XrdSysHeaders.hh
  XrdSys/XrdSysError.cc         XrdSys/XrdSysError.hh
  XrdSys/XrdSysLogger.cc        XrdSys/XrdSysLogger.hh

  #-----------------------------------------------------------------------------
  # XrdOuc
  #-----------------------------------------------------------------------------
  XrdOuc/XrdOuca2x.cc           XrdOuc/XrdOuca2x.hh
  XrdOuc/XrdOucArgs.cc          XrdOuc/XrdOucArgs.hh
  XrdOuc/XrdOucCache.cc         XrdOuc/XrdOucCache.hh
  XrdOuc/XrdOucCacheData.cc     XrdOuc/XrdOucCacheData.hh
  XrdOuc/XrdOucCacheReal.cc     XrdOuc/XrdOucCacheReal.hh
                                XrdOuc/XrdOucCacheSlot.hh
  XrdOuc/XrdOucCRC.cc           XrdOuc/XrdOucCRC.hh
  XrdOuc/XrdOucEnv.cc           XrdOuc/XrdOucEnv.hh
                                XrdOuc/XrdOucHash.hh
                                XrdOuc/XrdOucHash.icc
  XrdOuc/XrdOucExport.cc        XrdOuc/XrdOucExport.hh
  XrdOuc/XrdOucHashVal.cc
  XrdOuc/XrdOucMsubs.cc         XrdOuc/XrdOucMsubs.hh
  XrdOuc/XrdOucName2Name.cc     XrdOuc/XrdOucName2Name.hh
  XrdOuc/XrdOucNList.cc         XrdOuc/XrdOucNList.hh
  XrdOuc/XrdOucNSWalk.cc        XrdOuc/XrdOucNSWalk.hh
  XrdOuc/XrdOucProg.cc          XrdOuc/XrdOucProg.hh
  XrdOuc/XrdOucPup.cc           XrdOuc/XrdOucPup.hh
  XrdOuc/XrdOucReqID.cc         XrdOuc/XrdOucReqID.hh
  XrdOuc/XrdOucStream.cc        XrdOuc/XrdOucStream.hh
  XrdOuc/XrdOucString.cc        XrdOuc/XrdOucString.hh
  XrdOuc/XrdOucSxeq.cc          XrdOuc/XrdOucSxeq.hh
  XrdOuc/XrdOucTokenizer.cc     XrdOuc/XrdOucTokenizer.hh
  XrdOuc/XrdOucTrace.cc         XrdOuc/XrdOucTrace.hh
  XrdOuc/XrdOucUtils.cc         XrdOuc/XrdOucUtils.hh
                                XrdOuc/XrdOucChain.hh
                                XrdOuc/XrdOucDLlist.hh
                                XrdOuc/XrdOucErrInfo.hh
                                XrdOuc/XrdOucLock.hh
                                XrdOuc/XrdOucPList.hh
                                XrdOuc/XrdOucRash.hh
                                XrdOuc/XrdOucRash.icc
                                XrdOuc/XrdOucTable.hh
                                XrdOuc/XrdOucTList.hh
                                XrdOuc/XrdOucXAttr.hh

  #-----------------------------------------------------------------------------
  # XrdNet
  #-----------------------------------------------------------------------------
  XrdNet/XrdNet.cc              XrdNet/XrdNet.hh
                                XrdNet/XrdNetOpts.hh
                                XrdNet/XrdNetPeer.hh
  XrdNet/XrdNetBuffer.cc        XrdNet/XrdNetBuffer.hh
  XrdNet/XrdNetCmsNotify.cc     XrdNet/XrdNetCmsNotify.hh
  XrdNet/XrdNetConnect.cc       XrdNet/XrdNetConnect.hh
  XrdNet/XrdNetLink.cc          XrdNet/XrdNetLink.hh
  XrdNet/XrdNetMsg.cc           XrdNet/XrdNetMsg.hh
  XrdNet/XrdNetSecurity.cc      XrdNet/XrdNetSecurity.hh
  XrdNet/XrdNetSocket.cc        XrdNet/XrdNetSocket.hh
  XrdNet/XrdNetWork.cc          XrdNet/XrdNetWork.hh

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
  Xrd/XrdProtLoad.cc            Xrd/XrdProtLoad.hh
  Xrd/XrdProtocol.cc            Xrd/XrdProtocol.hh
  Xrd/XrdScheduler.cc           Xrd/XrdScheduler.hh
  Xrd/XrdStats.cc               Xrd/XrdStats.hh
                                Xrd/XrdTrace.hh
)

target_link_libraries( XrdUtils pthread dl ${SOCKET_LIBRARY} ${EXTRA_LIBS})

set_target_properties(
  XrdUtils
  PROPERTIES
  VERSION   ${XRD_UTILS_VERSION}
  SOVERSION ${XRD_UTILS_SOVERSION} )

#-------------------------------------------------------------------------------
# The helper lib
#-------------------------------------------------------------------------------
add_library(
  XrdProtocolLoader
  SHARED
  Xrd/XrdConfig.cc          Xrd/XrdConfig.hh
  Xrd/XrdMain.cc )

target_link_libraries( XrdProtocolLoader XrdUtils )

set_target_properties(
  XrdProtocolLoader
  PROPERTIES
  VERSION   ${XRD_PROTOCOL_LOADER_VERSION}
  SOVERSION ${XRD_PROTOCOL_LOADER_SOVERSION} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdUtils XrdProtocolLoader
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdSys/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdSys
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  DIRECTORY      XrdOuc/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdOuc
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc"
  PATTERN "*Bonjour*.hh" EXCLUDE )

install(
  DIRECTORY      XrdNet/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdNet
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  DIRECTORY      XrdSut/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdSut
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  DIRECTORY      Xrd/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/Xrd
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

# FIXME: Bonjour
