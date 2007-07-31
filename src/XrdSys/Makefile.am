#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

xrootdsysincdir = $(includedir)/xrootd/XrdSys

xrootdsysinc_HEADERS = XrdSysDir.hh XrdSysError.hh XrdSysLogger.hh
                       XrdSysPlugin.hh XrdSysPriv.hh \
                       XrdSysPlatform.hh XrdSysPthread.hh \
                       XrdSysSemWait.hh XrdSysTimer.hh XrdSysXSLock.hh

lib_LTLIBRARIES = libXrdSys.la

INCLUDES = -I$(top_srcdir)/src

libXrdSys_la_SOURCES = \
        XrdSysDir.cc XrdSysDir.hh \
        XrdSysError.cc XrdSysError.hh      \
        XrdSysLogger.cc XrdSysLogger.hh     \
        XrdSysPlugin.cc XrdSysPlugin.hh \
        XrdSysPriv.cc XrdSysPriv.hh \
        XrdSysPlatform.cc \
        XrdSysPthread.cc XrdSysPthread.hh \
        XrdSysSemWait.hh \
        XrdSysTimer.cc XrdSysTimer.hh \
        XrdSysXSLock.cc XrdSysXSLock.hh

libXrdSys_la_LIBADD = $(LIBPTHREAD) $(SEM_LIBRT) $(LIBNSL)
libXrdSys_la_CXXFLAGS = -prefer-pic
libXrdSys_la_LDFLAGS = -static

EXTRA_DIST = XrdWin32.cc  XrdWin32.hh
