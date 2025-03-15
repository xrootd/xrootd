#-------------------------------------------------------------------------------
# The Open File System and its dependencies
#-------------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdOfs.cc          XrdOfs.hh
    XrdOfsChkPnt.cc    XrdOfsChkPnt.hh
    XrdOfsConfig.cc
    XrdOfsConfigCP.cc  XrdOfsConfigCP.hh
    XrdOfsConfigPI.cc  XrdOfsConfigPI.hh
    XrdOfsCPFile.cc    XrdOfsCPFile.hh
    XrdOfsEvr.cc       XrdOfsEvr.hh
    XrdOfsEvs.cc       XrdOfsEvs.hh
    XrdOfsFAttr.cc
    XrdOfsFS.cc
    XrdOfsFSctl.cc
                       XrdOfsFSctl_PI.hh
    XrdOfsHandle.cc    XrdOfsHandle.hh
    XrdOfsPoscq.cc     XrdOfsPoscq.hh
                       XrdOfsSecurity.hh
    XrdOfsStats.cc     XrdOfsStats.hh
    XrdOfsTPC.cc       XrdOfsTPC.hh
    XrdOfsTPCAuth.cc   XrdOfsTPCAuth.hh
                       XrdOfsTPCConfig.hh
    XrdOfsTPCJob.cc    XrdOfsTPCJob.hh
    XrdOfsTPCInfo.cc   XrdOfsTPCInfo.hh
    XrdOfsTPCProg.cc   XrdOfsTPCProg.hh
                       XrdOfsTrace.hh
)
