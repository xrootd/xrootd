target_sources(XrdUtils
  PRIVATE
                          XrdSysAtomics.hh
    XrdSysDir.cc          XrdSysDir.hh
    XrdSysE2T.cc          XrdSysE2T.hh
    XrdSysError.cc        XrdSysError.hh
    XrdSysFAttr.cc        XrdSysFAttr.hh
                          XrdSysFAttrBsd.icc
                          XrdSysFAttrLnx.icc
                          XrdSysFAttrMac.icc
                          XrdSysFAttrSun.icc

                          XrdSysFD.hh
    XrdSysFallocate.cc    XrdSysFallocate.hh
                          XrdSysHeaders.hh
    XrdSysIOEvents.cc     XrdSysIOEvents.hh
                          XrdSysIOEventsPollE.icc
                          XrdSysIOEventsPollKQ.icc
                          XrdSysIOEventsPollPoll.icc
                          XrdSysIOEventsPollPort.icc
                          XrdSysLogPI.hh
    XrdSysLogger.cc       XrdSysLogger.hh
    XrdSysLogging.cc      XrdSysLogging.hh
                          XrdSysPageSize.hh
    XrdSysPlatform.cc     XrdSysPlatform.hh
    XrdSysPlugin.cc       XrdSysPlugin.hh
    XrdSysPriv.cc         XrdSysPriv.hh
    XrdSysPthread.cc      XrdSysPthread.hh
                          XrdSysRAtomic.hh
                          XrdSysSemWait.hh
    XrdSysTimer.cc        XrdSysTimer.hh
    XrdSysTrace.cc        XrdSysTrace.hh
    XrdSysUtils.cc        XrdSysUtils.hh
    XrdSysXAttr.cc        XrdSysXAttr.hh
    XrdSysXSLock.cc       XrdSysXSLock.hh
)
