#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

noinst_LTLIBRARIES = libXrdXr.la

bin_PROGRAMS = xrclient

INCLUDES = -I$(top_srcdir)/src

libXrdXr_la_SOURCES = \
        XrdXrClientWorker.cc XrdXrClientWorker.hh XrdXrTrace.hh \
        XrdXrClient.cc XrdXrClient.hh

xrclient_SOURCES = XrdXrMain.cc
xrclient_LDADD = -L../XrdSec -lXrdSec \
                 libXrdXr.la \
                 ../XrdNet/libXrdNet.la \
                 ../XrdOuc/libXrdOuc.la \
                 -ldl
