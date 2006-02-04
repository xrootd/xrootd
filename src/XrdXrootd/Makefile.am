#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

#       Note: since libXrdOfs needs to resolve symbols from xrootd's binary
#       itself, the xrootd binary must be compiled/linked with
#       -fPIC/-rdynamic (for gcc)

bin_PROGRAMS = xrootd

INCLUDES = -I$(top_srcdir)/src

xrootd_SOURCES = \
        XrdXrootdAdmin.cc XrdXrootdAdmin.hh \
        XrdXrootdAio.cc XrdXrootdAio.hh       \
        XrdXrootdConfig.cc XrdXrootdXPath.hh   \
        XrdXrootdFile.cc XrdXrootdFile.hh XrdXrootdFileLock.hh      \
        XrdXrootdFileLock1.cc XrdXrootdFileLock1.hh \
        XrdXrootdJob.cc XrdXrootdJob.hh \
        XrdXrootdLoadLib.cc \
        XrdXrootdMonitor.cc XrdXrootdMonitor.hh XrdXrootdMonData.hh \
        XrdXrootdPrepare.cc XrdXrootdPrepare.hh  \
        XrdXrootdProtocol.cc XrdXrootdProtocol.hh  \
        XrdXrootdResponse.cc XrdXrootdResponse.hh  \
        XrdXrootdStats.cc XrdXrootdStats.hh     \
        XrdXrootdXeq.cc        \
        XrdXrootdXeqAio.cc XrdXrootdTrace.hh

xrootd_LDADD = -ldl \
                  ../Xrd/libXrd.la \
                  ../XrdNet/libXrdNet.la \
                  ../XrdOuc/libXrdOuc.la \
                  ../XrdSfs/libXrdSfs.la

if IS_GCC
xrootd_CXXFLAGS = -fPIC
xrootd_LDFLAGS = -rdynamic -fPIC
endif


