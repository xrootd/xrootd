target_sources(XrdUtils
  PRIVATE
    XrdTls.cc             XrdTls.hh
    XrdTlsContext.cc      XrdTlsContext.hh
    XrdTlsHostcheck.icc
    XrdTlsHostcheck.hh
    XrdTlsNotary.cc       XrdTlsNotary.hh
    XrdTlsNotaryUtils.icc
    XrdTlsNotaryUtils.hh
    XrdTlsPeerCerts.cc    XrdTlsPeerCerts.hh
    XrdTlsSocket.cc       XrdTlsSocket.hh
    XrdTlsTempCA.cc       XrdTlsTempCA.hh
)
