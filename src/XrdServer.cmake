
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SERVER_VERSION   2.0.0 )
set( XRD_SERVER_SOVERSION 2 )
set( XRD_XROOTD_VERSION   2.0.0 )
set( XRD_XROOTD_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdClient lib
#-------------------------------------------------------------------------------
add_library(
  XrdServer
  SHARED

  #-----------------------------------------------------------------------------
  # XrdSfs
  #-----------------------------------------------------------------------------
  XrdSfs/XrdSfsNative.cc       XrdSfs/XrdSfsNative.hh
                               XrdSfs/XrdSfsAio.hh
                               XrdSfs/XrdSfsInterface.hh

  #-----------------------------------------------------------------------------
  # XrdFrc
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
  # XrdOss
  #-----------------------------------------------------------------------------
  XrdOss/XrdOssAio.cc
                               XrdOss/XrdOssTrace.hh
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
                               XrdOss/XrdOssUnlink.cc
                               XrdOss/XrdOssError.hh
                               XrdOss/XrdOss.hh

  #-----------------------------------------------------------------------------
  # XrdAcc
  #-----------------------------------------------------------------------------
  XrdAcc/XrdAccAccess.cc         XrdAcc/XrdAccAccess.hh
  XrdAcc/XrdAccAudit.cc          XrdAcc/XrdAccAudit.hh
                                 XrdAcc/XrdAccAuthDB.hh
                                 XrdAcc/XrdAccAuthorize.hh
  XrdAcc/XrdAccAuthFile.cc       XrdAcc/XrdAccAuthFile.hh
  XrdAcc/XrdAccCapability.cc     XrdAcc/XrdAccCapability.hh
  XrdAcc/XrdAccConfig.cc         XrdAcc/XrdAccConfig.hh
  XrdAcc/XrdAccGroups.cc         XrdAcc/XrdAccGroups.hh
                                 XrdAcc/XrdAccPrivs.hh

  #-----------------------------------------------------------------------------
  # XrdCms - client
  #-----------------------------------------------------------------------------
  XrdCms/XrdCmsBlackList.cc       XrdCms/XrdCmsBlackList.hh
  XrdCms/XrdCmsLogin.cc           XrdCms/XrdCmsLogin.hh
  XrdCms/XrdCmsParser.cc          XrdCms/XrdCmsParser.hh
  XrdCms/XrdCmsRRData.cc          XrdCms/XrdCmsRRData.hh
  XrdCms/XrdCmsSecurity.cc        XrdCms/XrdCmsSecurity.hh
  XrdCms/XrdCmsTalk.cc            XrdCms/XrdCmsTalk.hh
  XrdCms/XrdCmsClientConfig.cc    XrdCms/XrdCmsClientConfig.hh
  XrdCms/XrdCmsClientMan.cc       XrdCms/XrdCmsClientMan.hh
  XrdCms/XrdCmsClientMsg.cc       XrdCms/XrdCmsClientMsg.hh
  XrdCms/XrdCmsFinder.cc          XrdCms/XrdCmsFinder.hh
  XrdCms/XrdCmsClient.cc          XrdCms/XrdCmsClient.hh
  XrdCms/XrdCmsResp.cc            XrdCms/XrdCmsResp.hh
  XrdCms/XrdCmsReq.cc             XrdCms/XrdCmsReq.hh
  XrdCms/XrdCmsRTable.cc          XrdCms/XrdCmsRTable.hh
                                  XrdCms/XrdCmsTypes.hh
  XrdCms/XrdCmsUtils.cc           XrdCms/XrdCmsUtils.hh
                                  XrdCms/XrdCmsXmi.hh
  XrdCms/XrdCmsXmiReq.cc          XrdCms/XrdCmsXmiReq.hh
)

target_link_libraries(
  XrdServer
  XrdUtils
  pthread
  dl
  ${EXTRA_LIBS} )

set_target_properties(
  XrdServer
  PROPERTIES
  VERSION   ${XRD_SERVER_VERSION}
  SOVERSION ${XRD_SERVER_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# XrdXrootd
#-------------------------------------------------------------------------------
add_library(
  XrdXrootd
  SHARED
  XrdXrootd/XrdXrootdAdmin.cc           XrdXrootd/XrdXrootdAdmin.hh
  XrdXrootd/XrdXrootdAio.cc             XrdXrootd/XrdXrootdAio.hh
  XrdXrootd/XrdXrootdBridge.cc          XrdXrootd/XrdXrootdBridge.hh
  XrdXrootd/XrdXrootdCallBack.cc        XrdXrootd/XrdXrootdCallBack.hh
  XrdXrootd/XrdXrootdConfig.cc
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

  XrdXrootd/XrdXrootdPio.cc             XrdXrootd/XrdXrootdPio.hh
  XrdXrootd/XrdXrootdPrepare.cc         XrdXrootd/XrdXrootdPrepare.hh
  XrdXrootd/XrdXrootdProtocol.cc        XrdXrootd/XrdXrootdProtocol.hh
  XrdXrootd/XrdXrootdResponse.cc        XrdXrootd/XrdXrootdResponse.hh
                                        XrdXrootd/XrdXrootdStat.icc
  XrdXrootd/XrdXrootdStats.cc           XrdXrootd/XrdXrootdStats.hh
  XrdXrootd/XrdXrootdTransit.cc         XrdXrootd/XrdXrootdTransit.hh
  XrdXrootd/XrdXrootdTransPend.cc       XrdXrootd/XrdXrootdTransPend.hh
  XrdXrootd/XrdXrootdTransSend.cc       XrdXrootd/XrdXrootdTransSend.hh
  XrdXrootd/XrdXrootdXeq.cc
  XrdXrootd/XrdXrootdXeqAio.cc
                                        XrdXrootd/XrdXrootdTrace.hh
                                        XrdXrootd/XrdXrootdXPath.hh
                                        XrdXrootd/XrdXrootdReqID.hh )

target_link_libraries(
  XrdXrootd
  XrdOfs
  XrdUtils
  dl
  pthread
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

set_target_properties(
  XrdXrootd
  PROPERTIES
  VERSION   ${XRD_XROOTD_VERSION}
  SOVERSION ${XRD_XROOTD_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdServer XrdXrootd
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
