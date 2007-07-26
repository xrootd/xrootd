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

xrootdoucinc_HEADERS = \
    XrdOucChain.hh      XrdOucDLlist.hh \
    XrdOucEnv.hh        XrdOucRash.hh \
    XrdOucErrInfo.hh    XrdOucReqID.hh \
    XrdOucError.hh      XrdOuca2x.hh \
    XrdOucExport.hh     XrdOucStream.hh \
    XrdOucHash.hh       XrdOucString.hh \
    XrdOucLock.hh       XrdOucTList.hh \
    XrdOucLogger.hh     XrdOucTable.hh \
    XrdOucMsubs.hh      \
    XrdOucNList.hh      XrdOucTokenizer.hh \
    XrdOucName2Name.hh  XrdOucTrace.hh \
    XrdOucPList.hh      XrdOucUtils.hh \
    XrdOucHash.icc      XrdOucRash.icc

lib_LTLIBRARIES = libXrdOuc.la

INCLUDES = -I$(top_srcdir)/src

libXrdOuc_la_SOURCES = \
        XrdOuca2x.cc XrdOuca2x.hh        \
        XrdOucEnv.cc XrdOucEnv.hh XrdOucHash.hh       \
        XrdOucError.cc XrdOucError.hh      \
        XrdOucName2Name.cc XrdOucName2Name.hh \
        XrdOucHashVal.cc     \
        XrdOucLogger.cc XrdOucLogger.hh     \
        XrdOucMsubs.cc XrdOucMsubs.hh \
        XrdOucNList.cc XrdOucNList.hh      \
        XrdOucReqID.cc XrdOucReqID.hh      \
        XrdOucStream.cc XrdOucStream.hh     \
        XrdOucString.cc XrdOucString.hh     \
        XrdOucTokenizer.cc XrdOucTokenizer.hh  \
        XrdOucTrace.cc XrdOucTrace.hh      \
        XrdOucUtils.cc XrdOucUtils.hh      \
	XrdOucExport.cc XrdOucExport.hh \
        XrdOucHash.icc

libXrdOuc_la_LIBADD = $(LIBPTHREAD) $(SEM_LIBRT) $(LIBNSL)
#REFAC libXrdOuc_la_CXXFLAGS = -prefer-pic
#REFAC libXrdOuc_la_LDFLAGS = -static

