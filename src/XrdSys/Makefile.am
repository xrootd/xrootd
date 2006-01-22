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

xrootdsysinc_HEADERS = XrdSysDir.hh XrdSysPriv.hh

lib_LTLIBRARIES = libXrdSys.la

INCLUDES = -I$(top_srcdir)/src

libXrdSys_la_SOURCES = \
        XrdSysDir.cc XrdSysDir.hh \
        XrdSysPriv.cc XrdSysPriv.hh

libXrdSys_la_LIBADD = $(LIBPTHREAD) $(SEM_LIBRT) $(LIBNSL)
libXrdSys_la_CXXFLAGS = -prefer-pic
libXrdSys_la_LDFLAGS = -static

