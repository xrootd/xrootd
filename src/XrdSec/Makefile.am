#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################
xrootdsecincdir = $(includedir)/xrootd/XrdSec

xrootdsecinc_HEADERS = XrdSecInterface.hh XrdSecEntity.hh

lib_LTLIBRARIES = libXrdSec.la

bin_PROGRAMS = testclient testserver

INCLUDES = -I$(top_srcdir)/src

libXrdSec_la_SOURCES = \
        XrdSecClient.cc XrdSecInterface.hh XrdSecEntity.hh      \
        XrdSecPManager.cc XrdSecPManager.hh     \
        XrdSecProtocolhost.cc XrdSecProtocolhost.hh \
        XrdSecServer.cc XrdSecServer.hh XrdSecTrace.hh

testclient_SOURCES = XrdSectestClient.cc
testclient_LDADD =   libXrdSec.la \
                     -ldl \
                     ../XrdOuc/libXrdOuc.la \
                     ../XrdNet/libXrdNet.la

testserver_SOURCES = XrdSectestServer.cc
testserver_LDADD = libXrdSec.la \
                   -ldl \
                   ../XrdOuc/libXrdOuc.la \
                   ../XrdNet/libXrdNet.la
