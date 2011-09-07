
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SERVER_VERSION   0.0.1 )
set( XRD_SERVER_SOVERSION 0 )

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
  XrdOss/XrdOssStat.cc         XrdOss/XrdOssUnlink.cc
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
  XrdCms/XrdCmsLogin.cc           XrdCms/XrdCmsLogin.hh
  XrdCms/XrdCmsParser.cc          XrdCms/XrdCmsParser.hh
  XrdCms/XrdCmsRRData.cc          XrdCms/XrdCmsRRData.hh
  XrdCms/XrdCmsSecurity.cc        XrdCms/XrdCmsSecurity.hh
  XrdCms/XrdCmsTalk.cc            XrdCms/XrdCmsTalk.hh
  XrdCms/XrdCmsClientConfig.cc    XrdCms/XrdCmsClientConfig.hh
  XrdCms/XrdCmsClientMan.cc       XrdCms/XrdCmsClientMan.hh
  XrdCms/XrdCmsClientMsg.cc       XrdCms/XrdCmsClientMsg.hh
  XrdCms/XrdCmsFinder.cc          XrdCms/XrdCmsFinder.hh
                                  XrdCms/XrdCmsClient.hh
  XrdCms/XrdCmsResp.cc            XrdCms/XrdCmsResp.hh

  #-----------------------------------------------------------------------------
  # XrdCks
  #-----------------------------------------------------------------------------
  XrdCks/XrdCksCalccrc32.cc        XrdCks/XrdCksCalccrc32.hh
  XrdCks/XrdCksCalcmd5.cc          XrdCks/XrdCksCalcmd5.hh
  XrdCks/XrdCksConfig.cc           XrdCks/XrdCksConfig.hh
  XrdCks/XrdCksManager.cc          XrdCks/XrdCksManager.hh
                                   XrdCks/XrdCksCalcadler32.hh
                                   XrdCks/XrdCksCalc.hh
                                   XrdCks/XrdCksData.hh
                                   XrdCks/XrdCks.hh
                                   XrdCks/XrdCksXAttr.hh
)

# OSS
# FIXME: aio (librt)

# FIXME: defines
#XrdOssAio.cc:#if defined(_POSIX_ASYNCHRONOUS_IO) && !defined(HAVE_SIGWTI)
#XrdOssAio.cc:#if defined( _POSIX_ASYNCHRONOUS_IO) && !defined(HAVE_SIGWTI)

target_link_libraries(
  XrdServer
  XrdUtils
  ${EXTRA_LIBS} )

set_target_properties(
  XrdServer
  PROPERTIES
  VERSION   ${XRD_SERVER_VERSION}
  SOVERSION ${XRD_SERVER_SOVERSION} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdServer
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdSfs/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdSfs
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  DIRECTORY      XrdFrc/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdFrc
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  DIRECTORY      XrdOss/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdOss
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  DIRECTORY      XrdAcc/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdAcc
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

install(
  FILES
  XrdCms/XrdCmsLogin.hh
  XrdCms/XrdCmsParser.hh
  XrdCms/XrdCmsRRData.hh
  XrdCms/XrdCmsSecurity.hh
  XrdCms/XrdCmsTalk.hh
  XrdCms/XrdCmsClientConfig.hh
  XrdCms/XrdCmsClientMan.hh
  XrdCms/XrdCmsClientMsg.hh
  XrdCms/XrdCmsFinder.hh
  XrdCms/XrdCmsClient.hh
  XrdCms/XrdCmsResp.hh
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdCms
  PERMISSIONS OWNER_READ OWNER_WRITE )

install(
  DIRECTORY      XrdCks/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdCks
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
