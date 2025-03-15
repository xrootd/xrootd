#-----------------------------------------------------------------------------
# XrdSfs - Standard File System (basic)
#-----------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdSfsNative.cc       XrdSfsNative.hh
                          XrdSfsAio.hh
                          XrdSfsFAttr.hh
                          XrdSfsFlags.hh
                          XrdSfsGPFile.hh
    XrdSfsInterface.cc    XrdSfsInterface.hh
    XrdSfsXio.cc          XrdSfsXio.hh
                          XrdSfsXioImpl.hh
)
