#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

## Note: XrdOucRash.hh and XrdOucRash.icc seem not to be used
EXTRA_DIST = XrdOucRash.hh XrdOucRash.icc

xrootdoucincdir = $(includedir)/xrootd/XrdOuc

xrootdoucinc_HEADERS = XrdOucChain.hh XrdOucDLlist.hh XrdOucEnv.hh XrdOucErrInfo.hh \
                       XrdOucError.hh XrdOucHash.hh XrdOucLock.hh XrdOucLogger.hh \
                       XrdOucNList.hh XrdOucPList.hh XrdOucPlatform.hh XrdOucProg.hh \
                       XrdOucPthread.hh XrdOucRash.hh XrdOucReqID.hh XrdOucSemWait.hh \
                       XrdOucStream.hh XrdOucString.hh XrdOucTList.hh XrdOucTimer.hh \
                       XrdOucTokenizer.hh XrdOucTrace.hh XrdOucUtils.hh XrdOucXSLock.hh \
                       XrdOuca2x.hh 

lib_LTLIBRARIES = libXrdOuc.la

INCLUDES = -I$(top_srcdir)/src

libXrdOuc_la_SOURCES = \
        XrdOuca2x.cc XrdOuca2x.hh        \
        XrdOucEnv.cc XrdOucEnv.hh XrdOucHash.hh       \
        XrdOucError.cc XrdOucError.hh      \
        XrdOucHashVal.cc     \
        XrdOucLogger.cc XrdOucLogger.hh     \
        XrdOucNList.cc XrdOucNList.hh      \
        XrdOucPlatform.cc XrdOucPlatform.hh   \
        XrdOucProg.cc XrdOucProg.hh       \
        XrdOucPthread.cc XrdOucPthread.hh    \
        XrdOucReqID.cc XrdOucReqID.hh      \
        XrdOucStream.cc XrdOucStream.hh     \
        XrdOucString.cc XrdOucString.hh     \
        XrdOucTimer.cc XrdOucTimer.hh      \
        XrdOucTokenizer.cc XrdOucTokenizer.hh  \
        XrdOucTrace.cc XrdOucTrace.hh      \
        XrdOucUtils.cc XrdOucUtils.hh      \
        XrdOucXSLock.cc XrdOucXSLock.hh \
        XrdOucHash.icc

libXrdOuc_la_LIBADD = $(LIBPTHREAD) $(SEM_LIBRT) $(LIBNSL)
libXrdOuc_la_CXXFLAGS = -prefer-pic
libXrdOuc_la_LDFLAGS = -static

