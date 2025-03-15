#-----------------------------------------------------------------------------
# XrdFrc - File Residency Manager client
#-----------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdFrcCID.cc       XrdFrcCID.hh
    XrdFrcProxy.cc     XrdFrcProxy.hh
    XrdFrcReqAgent.cc  XrdFrcReqAgent.hh
    XrdFrcReqFile.cc   XrdFrcReqFile.hh
    XrdFrcTrace.cc     XrdFrcTrace.hh
    XrdFrcUtils.cc     XrdFrcUtils.hh
                       XrdFrcRequest.hh
                       XrdFrcXAttr.hh
                       XrdFrcXLock.hh
)
