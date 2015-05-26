
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SSI_VERSION   1.0.0 )
set( XRD_SSI_SOVERSION 1 )

#-------------------------------------------------------------------------------
# The XrdSsi library
#-------------------------------------------------------------------------------
add_library(
  XrdSsi
  SHARED
                                       XrdSsi/XrdSsiBVec.hh
                                       XrdSsi/XrdSsiCluster.hh
XrdSsi/XrdSsiCms.cc                    XrdSsi/XrdSsiCms.hh
                                       XrdSsi/XrdSsiErrInfo.hh
XrdSsi/XrdSsiFile.cc                   XrdSsi/XrdSsiFile.hh
XrdSsi/XrdSsiFileReq.cc                XrdSsi/XrdSsiFileReq.hh
XrdSsi/XrdSsiGCS.cc
XrdSsi/XrdSsiLogger.cc                 XrdSsi/XrdSsiLogger.hh
                                       XrdSsi/XrdSsiRRInfo.hh
                                       XrdSsi/XrdSsiRRTable.hh
                                       XrdSsi/XrdSsiRequest.hh
                                       XrdSsi/XrdSsiResponder.hh
XrdSsi/XrdSsiServReal.cc               XrdSsi/XrdSsiServReal.hh
                                       XrdSsi/XrdSsiService.hh
XrdSsi/XrdSsiSessReal.cc               XrdSsi/XrdSsiSessReal.hh
                                       XrdSsi/XrdSsiSession.hh
XrdSsi/XrdSsiSfs.cc                    XrdSsi/XrdSsiSfs.hh
XrdSsi/XrdSsiSfsConfig.cc              XrdSsi/XrdSsiSfsConfig.hh
XrdSsi/XrdSsiStat.cc
                                       XrdSsi/XrdSsiStream.hh
XrdSsi/XrdSsiTaskReal.cc               XrdSsi/XrdSsiTaskReal.hh
                                       XrdSsi/XrdSsiTrace.hh)

target_link_libraries(
  XrdSsi
  XrdCl
  XrdServer
  XrdUtils
  pthread )

set_target_properties(
  XrdSsi
  PROPERTIES
  VERSION   ${XRD_SSI_VERSION}
  SOVERSION ${XRD_SSI_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdSsi
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
