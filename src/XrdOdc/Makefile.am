#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

noinst_LTLIBRARIES = libXrdOdc.la

bin_PROGRAMS = XrdOdcTest

INCLUDES = -I$(top_srcdir)/src

libXrdOdc_la_SOURCES = \
        XrdOdcConfig.cc XrdOdcConfig.hh XrdOdcConfDefs.hh \
        XrdOdcFinder.cc XrdOdcFinder.hh \
        XrdOdcManager.cc XrdOdcManager.hh \
        XrdOdcMsg.cc XrdOdcMsg.hh XrdOdcTrace.hh

XrdOdcTest_SOURCES = XrdOdcTest.cc
XrdOdcTest_LDADD = libXrdOdc.la \
                   ../XrdNet/libXrdNet.la \
                   ../XrdOuc/libXrdOuc.la
