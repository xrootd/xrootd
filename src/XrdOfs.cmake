
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_OFS_VERSION   2.0.0 )
set( XRD_OFS_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The Open File System and its dependencies
#-------------------------------------------------------------------------------
add_library(
  XrdOfs
  SHARED
  XrdOfs/XrdOfs.cc              XrdOfs/XrdOfs.hh
                                XrdOfs/XrdOfsSecurity.hh
                                XrdOfs/XrdOfsTrace.hh
  XrdOfs/XrdOfsFS.cc
  XrdOfs/XrdOfsConfig.cc
  XrdOfs/XrdOfsEvr.cc           XrdOfs/XrdOfsEvr.hh
  XrdOfs/XrdOfsEvs.cc           XrdOfs/XrdOfsEvs.hh
  XrdOfs/XrdOfsHandle.cc        XrdOfs/XrdOfsHandle.hh
  XrdOfs/XrdOfsPoscq.cc         XrdOfs/XrdOfsPoscq.hh
  XrdOfs/XrdOfsStats.cc         XrdOfs/XrdOfsStats.hh
  XrdOfs/XrdOfsTPC.cc           XrdOfs/XrdOfsTPC.hh
  XrdOfs/XrdOfsTPCAuth.cc       XrdOfs/XrdOfsTPCAuth.hh
  XrdOfs/XrdOfsTPCJob.cc        XrdOfs/XrdOfsTPCJob.hh
  XrdOfs/XrdOfsTPCInfo.cc       XrdOfs/XrdOfsTPCInfo.hh
  XrdOfs/XrdOfsTPCProg.cc       XrdOfs/XrdOfsTPCProg.hh

  #-----------------------------------------------------------------------------
  # XrdSfs - Standard File System (basic)
  #-----------------------------------------------------------------------------
  XrdSfs/XrdSfsNative.cc       XrdSfs/XrdSfsNative.hh
                               XrdSfs/XrdSfsAio.hh
                               XrdSfs/XrdSfsFlags.hh
                               XrdSfs/XrdSfsInterface.hh

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
  # XrdAcc - Authorization
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
  # XrdCms - client for clustering
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

  #-----------------------------------------------------------------------------
  # XrdDig
  #-----------------------------------------------------------------------------
  XrdDig/XrdDigAuth.cc          XrdDig/XrdDigAuth.hh
  XrdDig/XrdDigConfig.cc        XrdDig/XrdDigConfig.hh
  XrdDig/XrdDigFS.cc            XrdDig/XrdDigFS.hh )

target_link_libraries(
  XrdOfs
  XrdUtils
  pthread )

set_target_properties(
  XrdOfs
  PROPERTIES
  VERSION   ${XRD_OFS_VERSION}
  SOVERSION ${XRD_OFS_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdOfs
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
