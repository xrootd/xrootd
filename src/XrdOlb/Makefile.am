#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

bin_PROGRAMS = olbd

INCLUDES = -I$(top_srcdir)/src

olbd_SOURCES = \
        XrdOlbAdmin.cc XrdOlbAdmin.hh XrdOlbTrace.hh   \
        XrdOlbCache.cc XrdOlbCache.hh    \
        XrdOlbConfig.cc XrdOlbConfig.hh XrdOlbTypes.hh  \
        XrdOlbMain.cc    \
        XrdOlbManager.cc XrdOlbManager.hh  \
        XrdOlbManList.cc XrdOlbManList.hh  \
        XrdOlbMeter.cc XrdOlbMeter.hh    \
        XrdOlbPList.cc XrdOlbPList.hh    \
        XrdOlbPrepare.cc XrdOlbPrepare.hh  \
        XrdOlbScheduler.cc XrdOlbScheduler.hh \
        XrdOlbServer.cc XrdOlbServer.hh   \
        XrdOlbState.cc XrdOlbState.hh

olbd_LDADD = ../XrdNet/libXrdNet.la \
             ../XrdOuc/libXrdOuc.la \
             -ldl


