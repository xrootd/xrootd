#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdSecgsi.la

bin_PROGRAMS = xrdgsiproxy

check_PROGRAMS = xrdsecgsitest

INCLUDES = -I$(top_srcdir)/src

libXrdSecgsi_la_SOURCES = \
        XrdSecProtocolgsi.cc XrdSecProtocolgsi.hh XrdSecgsiTrace.hh
libXrdSecgsi_la_LIBADD = ../XrdCrypto/libXrdCrypto.la


xrdgsiproxy_SOURCES = XrdSecgsiProxy.cc
xrdgsiproxy_LDADD = libXrdSecgsi.la \
                    ../XrdSec/libXrdSec.la \
                    ../XrdOuc/libXrdOuc.la \
                    ../XrdNet/libXrdNet.la

xrdsecgsitest_SOURCES = XrdSecgsitest.cc
xrdsecgsitest_LDADD = libXrdSecgsi.la \
                     ../XrdSec/libXrdSec.la \
                     ../XrdOuc/libXrdOuc.la \
                     ../XrdNet/libXrdNet.la

