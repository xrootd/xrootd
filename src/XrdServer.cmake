
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Plugin version (this protocol loaded eithr as a plugin or as builtin).
#-------------------------------------------------------------------------------
set( LIB_XRD_PROTOCOL XrdXrootd-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SERVER_VERSION   3.0.0 )
set( XRD_SERVER_SOVERSION 3 )

#-------------------------------------------------------------------------------
# The XRootD protocol implementation
#-------------------------------------------------------------------------------
add_library(
  XrdServer
  SHARED
  XrdXrootd/XrdXrootdAdmin.cc           XrdXrootd/XrdXrootdAdmin.hh
  XrdXrootd/XrdXrootdAioBuff.cc         XrdXrootd/XrdXrootdAioBuff.hh
  XrdXrootd/XrdXrootdAioFob.cc          XrdXrootd/XrdXrootdAioFob.hh
  XrdXrootd/XrdXrootdAioPgrw.cc         XrdXrootd/XrdXrootdAioPgrw.hh
  XrdXrootd/XrdXrootdAioTask.cc         XrdXrootd/XrdXrootdAioTask.hh
  XrdXrootd/XrdXrootdBridge.cc          XrdXrootd/XrdXrootdBridge.hh
  XrdXrootd/XrdXrootdCallBack.cc        XrdXrootd/XrdXrootdCallBack.hh
  XrdXrootd/XrdXrootdConfig.cc
  XrdXrootd/XrdXrootdConfigMon.cc
  XrdXrootd/XrdXrootdFile.cc            XrdXrootd/XrdXrootdFile.hh
                                        XrdXrootd/XrdXrootdFileLock.hh
  XrdXrootd/XrdXrootdFileLock1.cc       XrdXrootd/XrdXrootdFileLock1.hh
                                        XrdXrootd/XrdXrootdFileStats.hh
  XrdXrootd/XrdXrootdJob.cc             XrdXrootd/XrdXrootdJob.hh
  XrdXrootd/XrdXrootdLoadLib.cc
                                        XrdXrootd/XrdXrootdMonData.hh
  XrdXrootd/XrdXrootdMonFile.cc         XrdXrootd/XrdXrootdMonFile.hh
  XrdXrootd/XrdXrootdMonFMap.cc         XrdXrootd/XrdXrootdMonFMap.hh
  XrdXrootd/XrdXrootdMonitor.cc         XrdXrootd/XrdXrootdMonitor.hh

  XrdXrootd/XrdXrootdNormAio.cc         XrdXrootd/XrdXrootdNormAio.hh
  XrdXrootd/XrdXrootdPgrwAio.cc         XrdXrootd/XrdXrootdPgrwAio.hh
  XrdXrootd/XrdXrootdPgwBadCS.cc        XrdXrootd/XrdXrootdPgwBadCS.hh
  XrdXrootd/XrdXrootdPgwCtl.cc          XrdXrootd/XrdXrootdPgwCtl.hh
  XrdXrootd/XrdXrootdPgwFob.cc          XrdXrootd/XrdXrootdPgwFob.hh
  XrdXrootd/XrdXrootdPio.cc             XrdXrootd/XrdXrootdPio.hh
  XrdXrootd/XrdXrootdPrepare.cc         XrdXrootd/XrdXrootdPrepare.hh
  XrdXrootd/XrdXrootdProtocol.cc        XrdXrootd/XrdXrootdProtocol.hh
                                        XrdXrootd/XrdXrootdReqID.hh
  XrdXrootd/XrdXrootdResponse.cc        XrdXrootd/XrdXrootdResponse.hh
  XrdXrootd/XrdXrootdStats.cc           XrdXrootd/XrdXrootdStats.hh
  XrdXrootd/XrdXrootdGSReal.cc          XrdXrootd/XrdXrootdGSReal.hh
  XrdXrootd/XrdXrootdGStream.cc         XrdXrootd/XrdXrootdGStream.hh
                                        XrdXrootd/XrdXrootdTrace.hh
  XrdXrootd/XrdXrootdTransit.cc         XrdXrootd/XrdXrootdTransit.hh
  XrdXrootd/XrdXrootdTransPend.cc       XrdXrootd/XrdXrootdTransPend.hh
  XrdXrootd/XrdXrootdTransSend.cc       XrdXrootd/XrdXrootdTransSend.hh
                                        XrdXrootd/XrdXrootdWVInfo.hh
  XrdXrootd/XrdXrootdXeq.cc             XrdXrootd/XrdXrootdXeq.hh
  XrdXrootd/XrdXrootdXeqChkPnt.cc
  XrdXrootd/XrdXrootdXeqFAttr.cc
  XrdXrootd/XrdXrootdXeqPgrw.cc
                                        XrdXrootd/XrdXrootdXPath.hh


#-------------------------------------------------------------------------------
# The Open File System and its dependencies
#-------------------------------------------------------------------------------
  XrdOfs/XrdOfs.cc              XrdOfs/XrdOfs.hh
  XrdOfs/XrdOfsChkPnt.cc        XrdOfs/XrdOfsChkPnt.hh
  XrdOfs/XrdOfsConfig.cc
  XrdOfs/XrdOfsConfigCP.cc      XrdOfs/XrdOfsConfigCP.hh
  XrdOfs/XrdOfsConfigPI.cc      XrdOfs/XrdOfsConfigPI.hh
  XrdOfs/XrdOfsCPFile.cc        XrdOfs/XrdOfsCPFile.hh
  XrdOfs/XrdOfsEvr.cc           XrdOfs/XrdOfsEvr.hh
  XrdOfs/XrdOfsEvs.cc           XrdOfs/XrdOfsEvs.hh
  XrdOfs/XrdOfsFAttr.cc
  XrdOfs/XrdOfsFS.cc
  XrdOfs/XrdOfsFSctl.cc
                                XrdOfs/XrdOfsFSctl_PI.hh
  XrdOfs/XrdOfsHandle.cc        XrdOfs/XrdOfsHandle.hh
  XrdOfs/XrdOfsPoscq.cc         XrdOfs/XrdOfsPoscq.hh
                                XrdOfs/XrdOfsSecurity.hh
  XrdOfs/XrdOfsStats.cc         XrdOfs/XrdOfsStats.hh
  XrdOfs/XrdOfsTPC.cc           XrdOfs/XrdOfsTPC.hh
  XrdOfs/XrdOfsTPCAuth.cc       XrdOfs/XrdOfsTPCAuth.hh
                                XrdOfs/XrdOfsTPCConfig.hh
  XrdOfs/XrdOfsTPCJob.cc        XrdOfs/XrdOfsTPCJob.hh
  XrdOfs/XrdOfsTPCInfo.cc       XrdOfs/XrdOfsTPCInfo.hh
  XrdOfs/XrdOfsTPCProg.cc       XrdOfs/XrdOfsTPCProg.hh
                                XrdOfs/XrdOfsTrace.hh

  #-----------------------------------------------------------------------------
  # XrdSfs - Standard File System (basic)
  #-----------------------------------------------------------------------------
  XrdSfs/XrdSfsNative.cc       XrdSfs/XrdSfsNative.hh
                               XrdSfs/XrdSfsAio.hh
                               XrdSfs/XrdSfsFAttr.hh
                               XrdSfs/XrdSfsFlags.hh
                               XrdSfs/XrdSfsGPFile.hh
  XrdSfs/XrdSfsInterface.cc    XrdSfs/XrdSfsInterface.hh
  XrdSfs/XrdSfsXio.cc          XrdSfs/XrdSfsXio.hh
                               XrdSfs/XrdSfsXioImpl.hh

  #-----------------------------------------------------------------------------
  # XrdFrc - File Residency Manager client
  #-----------------------------------------------------------------------------
  XrdFrc/XrdFrcCID.cc          XrdFrc/XrdFrcCID.hh
  XrdFrc/XrdFrcProxy.cc        XrdFrc/XrdFrcProxy.hh
  XrdFrc/XrdFrcReqAgent.cc     XrdFrc/XrdFrcReqAgent.hh
  XrdFrc/XrdFrcReqFile.cc      XrdFrc/XrdFrcReqFile.hh
  XrdFrc/XrdFrcTrace.cc        XrdFrc/XrdFrcTrace.hh
  XrdFrc/XrdFrcUtils.cc        XrdFrc/XrdFrcUtils.hh
                               XrdFrc/XrdFrcRequest.hh
                               XrdFrc/XrdFrcXAttr.hh
                               XrdFrc/XrdFrcXLock.hh

  #-----------------------------------------------------------------------------
  # XrdOss - Default storage system
  #-----------------------------------------------------------------------------
  XrdOss/XrdOss.cc             XrdOss/XrdOss.hh
  XrdOss/XrdOssAt.cc           XrdOss/XrdOssAt.hh
  XrdOss/XrdOssAio.cc
                               XrdOss/XrdOssError.hh
                               XrdOss/XrdOssDefaultSS.hh
  XrdOss/XrdOssApi.cc          XrdOss/XrdOssApi.hh
  XrdOss/XrdOssCache.cc        XrdOss/XrdOssCache.hh
  XrdOss/XrdOssConfig.cc       XrdOss/XrdOssConfig.hh
  XrdOss/XrdOssCopy.cc         XrdOss/XrdOssCopy.hh
  XrdOss/XrdOssCreate.cc
                               XrdOss/XrdOssOpaque.hh
  XrdOss/XrdOssMio.cc          XrdOss/XrdOssMio.hh
                               XrdOss/XrdOssMioFile.hh
  XrdOss/XrdOssMSS.cc
  XrdOss/XrdOssPath.cc         XrdOss/XrdOssPath.hh
  XrdOss/XrdOssReloc.cc
  XrdOss/XrdOssRename.cc
  XrdOss/XrdOssSpace.cc        XrdOss/XrdOssSpace.hh
  XrdOss/XrdOssStage.cc        XrdOss/XrdOssStage.hh
  XrdOss/XrdOssStat.cc         XrdOss/XrdOssStatInfo.hh
                               XrdOss/XrdOssTrace.hh
  XrdOss/XrdOssUnlink.cc
                               XrdOss/XrdOssWrapper.hh
                               XrdOss/XrdOssVS.hh

  #-----------------------------------------------------------------------------
  # XrdAcc - Authorization
  #-----------------------------------------------------------------------------
  XrdAcc/XrdAccAccess.cc         XrdAcc/XrdAccAccess.hh
  XrdAcc/XrdAccAudit.cc          XrdAcc/XrdAccAudit.hh
                                 XrdAcc/XrdAccAuthDB.hh
                                 XrdAcc/XrdAccAuthorize.hh
  XrdAcc/XrdAccAuthFile.cc       XrdAcc/XrdAccAuthFile.hh
  XrdAcc/XrdAccCapability.cc     XrdAcc/XrdAccCapability.hh
  XrdAcc/XrdAccConfig.cc         XrdAcc/XrdAccConfig.hh
  XrdAcc/XrdAccEntity.cc         XrdAcc/XrdAccEntity.hh
  XrdAcc/XrdAccGroups.cc         XrdAcc/XrdAccGroups.hh
                                 XrdAcc/XrdAccPrivs.hh

  #-----------------------------------------------------------------------------
  # XrdCms - client for clustering
  #-----------------------------------------------------------------------------
  XrdCms/XrdCmsBlackList.cc       XrdCms/XrdCmsBlackList.hh
  XrdCms/XrdCmsClientConfig.cc    XrdCms/XrdCmsClientConfig.hh
  XrdCms/XrdCmsClientMan.cc       XrdCms/XrdCmsClientMan.hh
  XrdCms/XrdCmsClientMsg.cc       XrdCms/XrdCmsClientMsg.hh
  XrdCms/XrdCmsClient.cc          XrdCms/XrdCmsClient.hh
  XrdCms/XrdCmsFinder.cc          XrdCms/XrdCmsFinder.hh
  XrdCms/XrdCmsLogin.cc           XrdCms/XrdCmsLogin.hh
  XrdCms/XrdCmsParser.cc          XrdCms/XrdCmsParser.hh
                                  XrdCms/XrdCmsPerfMon.hh
  XrdCms/XrdCmsResp.cc            XrdCms/XrdCmsResp.hh
  XrdCms/XrdCmsRRData.cc          XrdCms/XrdCmsRRData.hh
  XrdCms/XrdCmsRTable.cc          XrdCms/XrdCmsRTable.hh
  XrdCms/XrdCmsSecurity.cc        XrdCms/XrdCmsSecurity.hh
  XrdCms/XrdCmsTalk.cc            XrdCms/XrdCmsTalk.hh
                                  XrdCms/XrdCmsTypes.hh
  XrdCms/XrdCmsUtils.cc           XrdCms/XrdCmsUtils.hh
                                  XrdCms/XrdCmsVnId.hh

  #-----------------------------------------------------------------------------
  # XrdDig
  #-----------------------------------------------------------------------------
  XrdDig/XrdDigAuth.cc          XrdDig/XrdDigAuth.hh
  XrdDig/XrdDigConfig.cc        XrdDig/XrdDigConfig.hh
  XrdDig/XrdDigFS.cc            XrdDig/XrdDigFS.hh )

target_link_libraries(
  XrdServer
  XrdUtils
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

set_target_properties(
  XrdServer
  PROPERTIES
  VERSION   ${XRD_SERVER_VERSION}
  SOVERSION ${XRD_SERVER_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XRootD protocol plugin
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_PROTOCOL}
  MODULE
  XrdXrootd/XrdXrootdPlugin.cc )

target_link_libraries(
  ${LIB_XRD_PROTOCOL}
  XrdServer
  XrdUtils
  ${EXTRA_LIBS} )

set_target_properties(
  ${LIB_XRD_PROTOCOL}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdServer ${LIB_XRD_PROTOCOL}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
