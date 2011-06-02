#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdCks.la

INCLUDES = -I$(top_srcdir)/src -I$(top_builddir)/src

xrootdcksincdir = $(includedir)/xrootd/XrdCks

xrootdcksinc_HEADERS = \
             XrdCks.hh          XrdCksCalcadler32.hh  XrdCksCalcmd5.hh \
             XrdCksData.hh      XrdCksXAttr.hh        XrdCksCalc.hh  \
             XrdCksCalccrc32.hh XrdCksConfig.hh       XrdCksManager.hh

libXrdCks_la_SOURCES = \
             XrdCksCalccrc32.cc  XrdCksCalcmd5.cc  XrdCksConfig.cc  \
             XrdCksManager.cc

libXrdCks_la_LIBTOOLFLAGS = --tag=disable-static
libXrdCks_la_LDFLAGS =

libXrdCks_la_LIBADD =
