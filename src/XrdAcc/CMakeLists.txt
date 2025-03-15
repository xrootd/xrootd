#-----------------------------------------------------------------------------
# XrdAcc - Authorization
#-----------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdAccAccess.cc      XrdAccAccess.hh
    XrdAccAudit.cc       XrdAccAudit.hh
                         XrdAccAuthDB.hh
                         XrdAccAuthorize.hh
    XrdAccAuthFile.cc    XrdAccAuthFile.hh
    XrdAccCapability.cc  XrdAccCapability.hh
    XrdAccConfig.cc      XrdAccConfig.hh
    XrdAccEntity.cc      XrdAccEntity.hh
    XrdAccGroups.cc      XrdAccGroups.hh
                         XrdAccPrivs.hh
)
