target_sources(XrdUtils
  PRIVATE
    XrdNet.cc           XrdNet.hh
                        XrdNetOpts.hh
                        XrdNetPeer.hh
                        XrdNetSockAddr.hh
    XrdNetAddr.cc       XrdNetAddr.hh
    XrdNetAddrInfo.cc   XrdNetAddrInfo.hh
    XrdNetBuffer.cc     XrdNetBuffer.hh
    XrdNetCache.cc      XrdNetCache.hh
    XrdNetCmsNotify.cc  XrdNetCmsNotify.hh
    XrdNetConnect.cc    XrdNetConnect.hh
    XrdNetIdentity.cc   XrdNetIdentity.hh
    XrdNetIF.cc         XrdNetIF.hh
    XrdNetMsg.cc        XrdNetMsg.hh
    XrdNetPMark.cc      XrdNetPMark.hh
    XrdNetPMarkCfg.cc   XrdNetPMarkCfg.hh
    XrdNetPMarkFF.cc    XrdNetPMarkFF.hh
    XrdNetRegistry.cc   XrdNetRegistry.hh
    XrdNetSecurity.cc   XrdNetSecurity.hh
    XrdNetSocket.cc     XrdNetSocket.hh
    XrdNetUtils.cc      XrdNetUtils.hh
)
