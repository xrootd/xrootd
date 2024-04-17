
install(
  FILES
  ${CMAKE_BINARY_DIR}/src/XrdVersion.hh
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xrootd )

set( XROOTD_PUBLIC_HEADERS
  XProtocol/XProtocol.hh
  XProtocol/XPtypes.hh
  Xrd/XrdBuffer.hh
  Xrd/XrdJob.hh
  Xrd/XrdLink.hh
  Xrd/XrdLinkMatch.hh
  Xrd/XrdProtocol.hh
  Xrd/XrdScheduler.hh
  Xrd/XrdTcpMonPin.hh
  XrdNet/XrdNet.hh
  XrdNet/XrdNetAddr.hh
  XrdNet/XrdNetAddrInfo.hh
  XrdNet/XrdNetUtils.hh
  XrdNet/XrdNetCmsNotify.hh
  XrdNet/XrdNetConnect.hh
  XrdNet/XrdNetOpts.hh
  XrdNet/XrdNetPMark.hh
  XrdNet/XrdNetSockAddr.hh
  XrdNet/XrdNetSocket.hh
  XrdOuc/XrdOucBuffer.hh
  XrdOuc/XrdOucCRC.hh
  XrdOuc/XrdOucCacheCM.hh
  XrdOuc/XrdOucCacheStats.hh
  XrdOuc/XrdOucCallBack.hh
  XrdOuc/XrdOucChain.hh
  XrdOuc/XrdOucDLlist.hh
  XrdOuc/XrdOucEnv.hh
  XrdOuc/XrdOucErrInfo.hh
  XrdOuc/XrdOucGMap.hh
  XrdOuc/XrdOucHash.hh
  XrdOuc/XrdOucHash.icc
  XrdOuc/XrdOucIOVec.hh
  XrdOuc/XrdOucLock.hh
  XrdOuc/XrdOucName2Name.hh
  XrdOuc/XrdOucPinPath.hh
  XrdOuc/XrdOucPinObject.hh
  XrdOuc/XrdOucRash.hh
  XrdOuc/XrdOucRash.icc
  XrdOuc/XrdOucSFVec.hh
  XrdOuc/XrdOucStream.hh
  XrdOuc/XrdOucString.hh
  XrdOuc/XrdOucTList.hh
  XrdOuc/XrdOucTable.hh
  XrdOuc/XrdOucTokenizer.hh
  XrdOuc/XrdOucTrace.hh
  XrdOuc/XrdOucUtils.hh
  XrdOuc/XrdOuca2x.hh
  XrdOuc/XrdOucEnum.hh
  XrdOuc/XrdOucCompiler.hh
  XrdPosix/XrdPosix.hh
  XrdPosix/XrdPosixCache.hh
  XrdPosix/XrdPosixCallBack.hh
  XrdPosix/XrdPosixExtern.hh
  XrdPosix/XrdPosixOsDep.hh
  XrdPosix/XrdPosixXrootd.hh
  XrdPosix/XrdPosixXrootdPath.hh
  XrdSec/XrdSecAttr.hh
  XrdSec/XrdSecEntity.hh
  XrdSec/XrdSecEntityAttr.hh
  XrdSec/XrdSecEntityPin.hh
  XrdSec/XrdSecInterface.hh
  XrdSys/XrdSysAtomics.hh
  XrdSys/XrdSysError.hh
  XrdSys/XrdSysFD.hh
  XrdSys/XrdSysHeaders.hh
  XrdSys/XrdSysLogger.hh
  XrdSys/XrdSysLogPI.hh
  XrdSys/XrdSysPageSize.hh
  XrdSys/XrdSysPlatform.hh
  XrdSys/XrdSysPlugin.hh
  XrdSys/XrdSysPthread.hh
  XrdSys/XrdSysSemWait.hh
  XrdSys/XrdSysTimer.hh
  XrdSys/XrdSysXAttr.hh
  XrdSys/XrdSysXSLock.hh
  XrdXml/XrdXmlReader.hh
  XrdXrootd/XrdXrootdMonData.hh
  XrdXrootd/XrdXrootdGStream.hh
  XrdXrootd/XrdXrootdBridge.hh
  XrdHttp/XrdHttpSecXtractor.hh
)

if( NOT XRDCL_ONLY )
  set( XROOTD_PUBLIC_HEADERS
    ${XROOTD_PUBLIC_HEADERS}
    XrdAcc/XrdAccAuthorize.hh
    XrdAcc/XrdAccPrivs.hh
    XrdCks/XrdCks.hh
    XrdCks/XrdCksAssist.hh
    XrdCks/XrdCksCalc.hh
    XrdCks/XrdCksData.hh
    XrdCks/XrdCksManager.hh
    XrdCks/XrdCksWrapper.hh
    XrdCms/XrdCmsClient.hh
    XrdCms/XrdCmsPerfMon.hh
    XrdCms/XrdCmsVnId.hh
    XrdPfc/XrdPfcDecision.hh
    XrdOfs/XrdOfsFSctl_PI.hh
    XrdOfs/XrdOfsPrepare.hh
    XrdOss/XrdOss.hh
    XrdOss/XrdOssVS.hh
    XrdOss/XrdOssDefaultSS.hh
    XrdOss/XrdOssStatInfo.hh
    XrdOss/XrdOssWrapper.hh
    XrdSfs/XrdSfsAio.hh
    XrdSfs/XrdSfsDio.hh
    XrdSfs/XrdSfsXio.hh
    XrdSfs/XrdSfsFlags.hh
    XrdSfs/XrdSfsGPFile.hh
    XrdSfs/XrdSfsInterface.hh
    XrdXrootd/XrdXrootdMonData.hh
    XrdXrootd/XrdXrootdBridge.hh
    XrdHttp/XrdHttpSecXtractor.hh
  )
endif()

set( XROOTD_PRIVATE_HEADERS
  Xrd/XrdPoll.hh
  XrdNet/XrdNetPeer.hh
  XrdNet/XrdNetBuffer.hh
  XrdNet/XrdNetIF.hh
  XrdSecsss/XrdSecsssID.hh
  XrdSys/XrdSysPriv.hh
  XrdOuc/XrdOucCRC32C.hh
  XrdOuc/XrdOucExport.hh
  XrdOuc/XrdOucGatherConf.hh
  XrdOuc/XrdOucPList.hh
  XrdOuc/XrdOucN2NLoader.hh
  XrdOuc/XrdOucPinLoader.hh
  XrdOuc/XrdOucTUtils.hh
  XrdPosix/XrdPosixMap.hh
  XrdZip/XrdZipCDFH.hh
  XrdZip/XrdZipDataDescriptor.hh
  XrdZip/XrdZipEOCD.hh
  XrdZip/XrdZipExtra.hh
  XrdZip/XrdZipLFH.hh
  XrdZip/XrdZipUtils.hh
  XrdZip/XrdZipZIP64EOCD.hh
  XrdZip/XrdZipZIP64EOCDL.hh
)

if( NOT XRDCL_ONLY )
  set( XROOTD_PRIVATE_HEADERS
    ${XROOTD_PRIVATE_HEADERS}
    XrdHttp/XrdHttpExtHandler.hh
    XrdSys/XrdSysTrace.hh
    XrdOfs/XrdOfs.hh
    XrdOfs/XrdOfsEvr.hh
    XrdOfs/XrdOfsHandle.hh
    XrdOfs/XrdOfsTrace.hh
    XrdOfs/XrdOfsTPCInfo.hh
    XrdSfs/XrdSfsFAttr.hh
    XrdSsi/XrdSsiAtomics.hh
    XrdSsi/XrdSsiCluster.hh
    XrdSsi/XrdSsiEntity.hh
    XrdSsi/XrdSsiErrInfo.hh
    XrdSsi/XrdSsiLogger.hh
    XrdSsi/XrdSsiProvider.hh
    XrdSsi/XrdSsiRequest.hh
    XrdSsi/XrdSsiRespInfo.hh
    XrdSsi/XrdSsiResponder.hh
    XrdSsi/XrdSsiResource.hh
    XrdSsi/XrdSsiService.hh
    XrdSsi/XrdSsiStream.hh

    XrdOss/XrdOssApi.hh
    XrdOss/XrdOssConfig.hh
    XrdOss/XrdOssError.hh

    XrdCrypto/XrdCryptoX509.hh
    XrdCrypto/XrdCryptoX509Chain.hh
    XrdCrypto/XrdCryptoAux.hh
    XrdCrypto/XrdCryptoFactory.hh
    XrdCrypto/XrdCryptosslAux.hh
    XrdCrypto/XrdCryptoX509Crl.hh
    XrdCrypto/XrdCryptoX509Req.hh
    XrdCrypto/XrdCryptoRSA.hh

    XrdSut/XrdSutAux.hh
    XrdSut/XrdSutBucket.hh

    XrdOuc/XrdOucPgrwUtils.hh
)

  if ( BUILD_VOMS )
    set( XROOTD_PRIVATE_HEADERS
      ${XROOTD_PRIVATE_HEADERS}
      XrdVoms/XrdVoms.hh
    )
  endif()
endif()

install_headers(
  ${CMAKE_INSTALL_INCLUDEDIR}/xrootd
  "${XROOTD_PUBLIC_HEADERS}" )

install_headers(
  ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/private
  "${XROOTD_PRIVATE_HEADERS}" )
