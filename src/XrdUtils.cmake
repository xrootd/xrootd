
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_UTILS_VERSION   3.0.0 )
set( XRD_UTILS_SOVERSION 3 )
set( XRD_ZCRC32_VERSION   3.0.0 )
set( XRD_ZCRC32_SOVERSION 3 )

#-------------------------------------------------------------------------------
# The XrdUtils library
#-------------------------------------------------------------------------------

  #-----------------------------------------------------------------------------
  # XProtocol
  #-----------------------------------------------------------------------------
set ( XProtocolSources
  XProtocol/XProtocol.cc        XProtocol/XProtocol.hh
)

  #-----------------------------------------------------------------------------
  # XrdSys
  #-----------------------------------------------------------------------------
set ( XrdSysSources
                                XrdSys/XrdSysAtomics.hh
  XrdSys/XrdSysDir.cc           XrdSys/XrdSysDir.hh
  XrdSys/XrdSysE2T.cc           XrdSys/XrdSysE2T.hh
  XrdSys/XrdSysError.cc         XrdSys/XrdSysError.hh
  XrdSys/XrdSysFAttr.cc         XrdSys/XrdSysFAttr.hh
                                XrdSys/XrdSysFAttrBsd.icc
                                XrdSys/XrdSysFAttrLnx.icc
                                XrdSys/XrdSysFAttrMac.icc
                                XrdSys/XrdSysFAttrSun.icc
                                XrdSys/XrdSysFD.hh
  XrdSys/XrdSysFallocate.cc     XrdSys/XrdSysFallocate.hh
                                XrdSys/XrdSysHeaders.hh
  XrdSys/XrdSysIOEvents.cc      XrdSys/XrdSysIOEvents.hh
                                XrdSys/XrdSysIOEventsPollE.icc
                                XrdSys/XrdSysIOEventsPollKQ.icc
                                XrdSys/XrdSysIOEventsPollPoll.icc
                                XrdSys/XrdSysIOEventsPollPort.icc
                                XrdSys/XrdSysLogPI.hh
  XrdSys/XrdSysLogger.cc        XrdSys/XrdSysLogger.hh
  XrdSys/XrdSysLogging.cc       XrdSys/XrdSysLogging.hh
                                XrdSys/XrdSysPageSize.hh
  XrdSys/XrdSysPlatform.cc      XrdSys/XrdSysPlatform.hh
  XrdSys/XrdSysPlugin.cc        XrdSys/XrdSysPlugin.hh
  XrdSys/XrdSysPriv.cc          XrdSys/XrdSysPriv.hh
  XrdSys/XrdSysPthread.cc       XrdSys/XrdSysPthread.hh
                                XrdSys/XrdSysRAtomic.hh
                                XrdSys/XrdSysSemWait.hh
  XrdSys/XrdSysTimer.cc         XrdSys/XrdSysTimer.hh
  XrdSys/XrdSysTrace.cc         XrdSys/XrdSysTrace.hh
  XrdSys/XrdSysUtils.cc         XrdSys/XrdSysUtils.hh
  XrdSys/XrdSysXAttr.cc         XrdSys/XrdSysXAttr.hh
  XrdSys/XrdSysXSLock.cc        XrdSys/XrdSysXSLock.hh
)

  #-----------------------------------------------------------------------------
  # XrdTls
  #-----------------------------------------------------------------------------
if ( WITH_OPENSSL3 )
  set ( XrdTlsSources
    XrdTls/XrdTls.cc                  XrdTls/XrdTls.hh
    XrdTls/openssl3/XrdTlsContext.cc  XrdTls/XrdTlsContext.hh
    XrdTls/XrdTlsHostcheck.icc        XrdTls/XrdTlsHostcheck.hh
    XrdTls/XrdTlsNotary.cc            XrdTls/XrdTlsNotary.hh
    XrdTls/XrdTlsNotaryUtils.icc      XrdTls/XrdTlsNotaryUtils.hh
    XrdTls/XrdTlsPeerCerts.cc         XrdTls/XrdTlsPeerCerts.hh
    XrdTls/XrdTlsSocket.cc            XrdTls/XrdTlsSocket.hh
    XrdTls/XrdTlsTempCA.cc            XrdTls/XrdTlsTempCA.hh
  )
else()
  set ( XrdTlsSources
    XrdTls/XrdTls.cc                  XrdTls/XrdTls.hh
    XrdTls/XrdTlsContext.cc           XrdTls/XrdTlsContext.hh
    XrdTls/XrdTlsHostcheck.icc        XrdTls/XrdTlsHostcheck.hh
    XrdTls/XrdTlsNotary.cc            XrdTls/XrdTlsNotary.hh
    XrdTls/XrdTlsNotaryUtils.icc      XrdTls/XrdTlsNotaryUtils.hh
    XrdTls/XrdTlsPeerCerts.cc         XrdTls/XrdTlsPeerCerts.hh
    XrdTls/XrdTlsSocket.cc            XrdTls/XrdTlsSocket.hh
    XrdTls/XrdTlsTempCA.cc            XrdTls/XrdTlsTempCA.hh
  )
endif()

  #-----------------------------------------------------------------------------
  # XrdCrypto: linking against a few functions that are useful for XrdTls; avoids
  # linking against all of libXrdCryptossl in XrdUtils
  #-----------------------------------------------------------------------------
if ( WITH_OPENSSL3 )
  set ( XrdCryptoSources
    XrdCrypto/openssl3/XrdCryptosslAux.cc     XrdCrypto/XrdCryptosslAux.hh
    XrdCrypto/openssl3/XrdCryptosslX509.cc    XrdCrypto/XrdCryptosslX509.hh
    XrdCrypto/XrdCryptoX509.cc                XrdCrypto/XrdCryptoX509.hh
    XrdCrypto/XrdCryptoX509Chain.cc           XrdCrypto/XrdCryptoX509Chain.hh
    XrdCrypto/openssl3/XrdCryptosslRSA.cc     XrdCrypto/XrdCryptosslRSA.hh
    XrdCrypto/XrdCryptoRSA.cc                 XrdCrypto/XrdCryptoRSA.hh
    XrdCrypto/openssl3/XrdCryptosslgsiAux.cc  XrdCrypto/XrdCryptosslgsiAux.hh
    XrdCrypto/XrdCryptosslX509Req.cc          XrdCrypto/XrdCryptosslX509Req.hh
    XrdCrypto/XrdCryptoX509Req.cc             XrdCrypto/XrdCryptoX509Req.hh
    XrdCrypto/XrdCryptoAux.cc                 XrdCrypto/XrdCryptoAux.hh
    XrdCrypto/XrdCryptosslX509Crl.cc          XrdCrypto/XrdCryptosslX509Crl.hh
    XrdCrypto/XrdCryptoX509Crl.cc             XrdCrypto/XrdCryptoX509Crl.hh
                                              XrdCrypto/XrdCryptoTrace.hh
  )
else()
  set ( XrdCryptoSources
    XrdCrypto/XrdCryptosslAux.cc              XrdCrypto/XrdCryptosslAux.hh
    XrdCrypto/XrdCryptosslX509.cc             XrdCrypto/XrdCryptosslX509.hh
    XrdCrypto/XrdCryptoX509.cc                XrdCrypto/XrdCryptoX509.hh
    XrdCrypto/XrdCryptoX509Chain.cc           XrdCrypto/XrdCryptoX509Chain.hh
    XrdCrypto/XrdCryptosslRSA.cc              XrdCrypto/XrdCryptosslRSA.hh
    XrdCrypto/XrdCryptoRSA.cc                 XrdCrypto/XrdCryptoRSA.hh
    XrdCrypto/XrdCryptosslgsiAux.cc           XrdCrypto/XrdCryptosslgsiAux.hh
    XrdCrypto/XrdCryptosslX509Req.cc          XrdCrypto/XrdCryptosslX509Req.hh
    XrdCrypto/XrdCryptoX509Req.cc             XrdCrypto/XrdCryptoX509Req.hh
    XrdCrypto/XrdCryptoAux.cc                 XrdCrypto/XrdCryptoAux.hh
    XrdCrypto/XrdCryptosslX509Crl.cc          XrdCrypto/XrdCryptosslX509Crl.hh
    XrdCrypto/XrdCryptoX509Crl.cc             XrdCrypto/XrdCryptoX509Crl.hh
                                              XrdCrypto/XrdCryptoTrace.hh
  )
endif()

  #-----------------------------------------------------------------------------
  # XrdOuc
  #-----------------------------------------------------------------------------
set ( XrdOucSources
  XrdOuc/XrdOuca2x.cc           XrdOuc/XrdOuca2x.hh
  XrdOuc/XrdOucArgs.cc          XrdOuc/XrdOucArgs.hh
  XrdOuc/XrdOucBackTrace.cc     XrdOuc/XrdOucBackTrace.hh
  XrdOuc/XrdOucBuffer.cc        XrdOuc/XrdOucBuffer.hh
  XrdOuc/XrdOucCache.cc         XrdOuc/XrdOucCache.hh
                                XrdOuc/XrdOucCacheStats.hh
  XrdOuc/XrdOucCallBack.cc      XrdOuc/XrdOucCallBack.hh
                                XrdOuc/XrdOucChkPnt.hh
  XrdOuc/XrdOucCRC.cc           XrdOuc/XrdOucCRC.hh
  XrdOuc/XrdOucCRC32C.cc        XrdOuc/XrdOucCRC32C.hh
  XrdOuc/XrdOucEnv.cc           XrdOuc/XrdOucEnv.hh
                                XrdOuc/XrdOucHash.hh
                                XrdOuc/XrdOucHash.icc
  XrdOuc/XrdOucERoute.cc        XrdOuc/XrdOucERoute.hh
                                XrdOuc/XrdOucErrInfo.hh
  XrdOuc/XrdOucExport.cc        XrdOuc/XrdOucExport.hh
  XrdOuc/XrdOucFileInfo.cc      XrdOuc/XrdOucFileInfo.hh
  XrdOuc/XrdOucGMap.cc          XrdOuc/XrdOucGMap.hh
  XrdOuc/XrdOucHashVal.cc
  XrdOuc/XrdOucLogging.cc       XrdOuc/XrdOucLogging.hh
  XrdOuc/XrdOucMsubs.cc         XrdOuc/XrdOucMsubs.hh
  XrdOuc/XrdOucName2Name.cc     XrdOuc/XrdOucName2Name.hh
  XrdOuc/XrdOucN2NLoader.cc     XrdOuc/XrdOucN2NLoader.hh
  XrdOuc/XrdOucNList.cc         XrdOuc/XrdOucNList.hh
  XrdOuc/XrdOucNSWalk.cc        XrdOuc/XrdOucNSWalk.hh
  XrdOuc/XrdOucPgrwUtils.cc     XrdOuc/XrdOucPgrwUtils.hh
                                XrdOuc/XrdOucPinKing.hh
  XrdOuc/XrdOucPinLoader.cc     XrdOuc/XrdOucPinLoader.hh
                                XrdOuc/XrdOucPinObject.hh
  XrdOuc/XrdOucPinPath.cc       XrdOuc/XrdOucPinPath.hh
  XrdOuc/XrdOucPreload.cc       XrdOuc/XrdOucPreload.hh
  XrdOuc/XrdOucProg.cc          XrdOuc/XrdOucProg.hh
  XrdOuc/XrdOucPsx.cc           XrdOuc/XrdOucPsx.hh
  XrdOuc/XrdOucPup.cc           XrdOuc/XrdOucPup.hh
  XrdOuc/XrdOucReqID.cc         XrdOuc/XrdOucReqID.hh
  XrdOuc/XrdOucSHA3.cc          XrdOuc/XrdOucSHA3.hh
  XrdOuc/XrdOucSid.cc           XrdOuc/XrdOucSid.hh
  XrdOuc/XrdOucSiteName.cc      XrdOuc/XrdOucSiteName.hh
  XrdOuc/XrdOucStream.cc        XrdOuc/XrdOucStream.hh
  XrdOuc/XrdOucString.cc        XrdOuc/XrdOucString.hh
  XrdOuc/XrdOucSxeq.cc          XrdOuc/XrdOucSxeq.hh
  XrdOuc/XrdOucTokenizer.cc     XrdOuc/XrdOucTokenizer.hh
  XrdOuc/XrdOucTPC.cc           XrdOuc/XrdOucTPC.hh
  XrdOuc/XrdOucTrace.cc         XrdOuc/XrdOucTrace.hh
  XrdOuc/XrdOucUri.cc           XrdOuc/XrdOucUri.hh
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
)

  #-----------------------------------------------------------------------------
  # XrdNet
  #-----------------------------------------------------------------------------
set ( XrdNetSources
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
  XrdNet/XrdNetRegistry.cc      XrdNet/XrdNetRegistry.hh
  XrdNet/XrdNetSecurity.cc      XrdNet/XrdNetSecurity.hh
  XrdNet/XrdNetSocket.cc        XrdNet/XrdNetSocket.hh
  XrdNet/XrdNetUtils.cc         XrdNet/XrdNetUtils.hh
)

  #-----------------------------------------------------------------------------
  # XrdSut
  #-----------------------------------------------------------------------------
set ( XrdSutSources
  XrdSut/XrdSutAux.cc           XrdSut/XrdSutAux.hh
  XrdSut/XrdSutPFCache.cc       XrdSut/XrdSutPFCache.hh
  XrdSut/XrdSutBucket.cc        XrdSut/XrdSutBucket.hh
  XrdSut/XrdSutBuckList.cc      XrdSut/XrdSutBuckList.hh
  XrdSut/XrdSutBuffer.cc        XrdSut/XrdSutBuffer.hh
  XrdSut/XrdSutPFile.cc         XrdSut/XrdSutPFile.hh
  XrdSut/XrdSutCacheEntry.cc    XrdSut/XrdSutCacheEntry.hh
  XrdSut/XrdSutPFEntry.cc       XrdSut/XrdSutPFEntry.hh
  XrdSut/XrdSutRndm.cc          XrdSut/XrdSutRndm.hh
  XrdSut/XrdSutTrace.hh
)

  #-----------------------------------------------------------------------------
  # Xrd
  #-----------------------------------------------------------------------------
set ( XrdSources
  Xrd/XrdBuffer.cc              Xrd/XrdBuffer.hh
  Xrd/XrdBuffXL.cc              Xrd/XrdBuffXL.hh
  Xrd/XrdInet.cc                Xrd/XrdInet.hh
  Xrd/XrdInfo.cc                Xrd/XrdInfo.hh
  Xrd/XrdJob.hh
  Xrd/XrdLink.cc                Xrd/XrdLink.hh
  Xrd/XrdLinkCtl.cc             Xrd/XrdLinkCtl.hh
                                Xrd/XrdLinkInfo.hh
  Xrd/XrdLinkXeq.cc             Xrd/XrdLinkXeq.hh
  Xrd/XrdLinkMatch.cc           Xrd/XrdLinkMatch.hh
  Xrd/XrdGlobals.cc
  Xrd/XrdObject.icc             Xrd/XrdObject.hh
  Xrd/XrdPoll.cc                Xrd/XrdPoll.hh
                                Xrd/XrdPollE.hh
                                Xrd/XrdPollE.icc
                                Xrd/XrdPollInfo.hh
                                Xrd/XrdPollPoll.hh
                                Xrd/XrdPollPoll.icc
                                Xrd/XrdProtocol.hh
  Xrd/XrdScheduler.cc           Xrd/XrdScheduler.hh
  Xrd/XrdSendQ.cc               Xrd/XrdSendQ.hh
                                Xrd/XrdTrace.hh
)

  #-----------------------------------------------------------------------------
  # XrdCks
  #-----------------------------------------------------------------------------
set ( XrdCksSources
  XrdCks/XrdCksAssist.cc           XrdCks/XrdCksAssist.hh
  XrdCks/XrdCksCalccrc32.cc        XrdCks/XrdCksCalccrc32.hh
  XrdCks/XrdCksCalccrc32C.cc       XrdCks/XrdCksCalccrc32C.hh
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
)

  #-----------------------------------------------------------------------------
  # XrdRmc
  #-----------------------------------------------------------------------------
set ( XrdRmcSources
  XrdRmc/XrdRmc.cc                 XrdRmc/XrdRmc.hh
  XrdRmc/XrdRmcData.cc             XrdRmc/XrdRmcData.hh
  XrdRmc/XrdRmcReal.cc             XrdRmc/XrdRmcReal.hh
                                   XrdRmc/XrdRmcSlot.hh
)

  #-----------------------------------------------------------------------------
  # XrdSec
  #-----------------------------------------------------------------------------
set ( XrdSecSources
  XrdSec/XrdSecEntity.cc           XrdSec/XrdSecEntity.hh
  XrdSec/XrdSecEntityAttr.cc       XrdSec/XrdSecEntityAttr.hh
  XrdSec/XrdSecEntityXtra.cc       XrdSec/XrdSecEntityXtra.hh
  XrdSec/XrdSecLoadSecurity.cc     XrdSec/XrdSecLoadSecurity.hh
  XrdSecsss/XrdSecsssCon.cc        XrdSecsss/XrdSecsssCon.hh
  XrdSecsss/XrdSecsssEnt.cc        XrdSecsss/XrdSecsssEnt.hh
  XrdSecsss/XrdSecsssID.cc         XrdSecsss/XrdSecsssID.hh
  XrdSecsss/XrdSecsssKT.cc         XrdSecsss/XrdSecsssKT.hh
                                   XrdSecsss/XrdSecsssMap.hh
)

add_library(
  XrdUtils
  SHARED
  ${XProtocolSources}
  ${XrdSysSources}
  ${XrdTlsSources}
  ${XrdCryptoSources}
  ${XrdOucSources}
  ${XrdNetSources}
  ${XrdSutSources}
  ${XrdSources}
  ${XrdCksSources}
  ${XrdRmcSources}
  ${XrdSecSources}
)

target_link_libraries(
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS}
  ${OPENSSL_LIBRARIES}
  ${SOCKET_LIBRARY}
  ${SENDFILE_LIBRARY}
  ${EXTRA_LIBS} )

if ( SYSTEMD_FOUND )
   target_link_libraries(
     XrdUtils
     ${SYSTEMD_LIBRARIES}
   )
endif()

set_target_properties(
  XrdUtils
  PROPERTIES
  BUILD_RPATH ${CMAKE_CURRENT_BINARY_DIR}
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
