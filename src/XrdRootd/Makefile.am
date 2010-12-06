#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

noinst_LTLIBRARIES = libXrdRootd.la

INCLUDES = -I$(top_srcdir)/src

libXrdRootd_la_SOURCES = XrdRootdProtocol.cc XrdRootdProtocol.hh
libXrdRootd_la_LIBADD = ../XrdOuc/libXrdOuc.la \
                        ../XrdSys/libXrdSys.la

