#-----------------------------------------------------------------------------
# XrdDig
#-----------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdDigAuth.cc    XrdDigAuth.hh
    XrdDigConfig.cc  XrdDigConfig.hh
    XrdDigFS.cc      XrdDigFS.hh
)
