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
        XrdOlbManager.cc XrdOlbManager.hh  \
        XrdOlbManList.cc XrdOlbManList.hh  \
        XrdOlbMeter.cc XrdOlbMeter.hh    \
        XrdOlbPList.cc XrdOlbPList.hh    \
        XrdOlbPrepare.cc XrdOlbPrepare.hh  \
        XrdOlbProtocol.cc XrdOlbProtocol.hh \
	XrdOlbReq.cc XrdOlbReq.hh \
        XrdOlbRRQ.cc XrdOlbRRQ.hh \
        XrdOlbRTable.cc XrdOlbRTable.hh \
        XrdOlbServer.cc XrdOlbServer.hh   \
        XrdOlbState.cc XrdOlbState.hh \
	XrdOlbXmiReq.cc XrdOlbXmiReq.hh XrdOlbXmi.hh

olbd_LDADD = ../XrdNet/libXrdNet.la \
             ../XrdOuc/libXrdOuc.la \
             ../Xrd/libXrd.la \
             -ldl


