#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

noinst_LTLIBRARIES = libXrdAcc.la

bin_PROGRAMS = XrdAccTest

INCLUDES = -I$(top_srcdir)/src

libXrdAcc_la_SOURCES = \
        XrdAccAccess.cc XrdAccAccess.hh     \
        XrdAccAudit.cc XrdAccAudit.hh      \
        XrdAccAuthFile.cc XrdAccAuthFile.hh   \
        XrdAccCapability.cc XrdAccCapability.hh \
        XrdAccConfig.cc XrdAccConfig.hh XrdAccAuthDB.hh    \
        XrdAccGroups.cc XrdAccGroups.hh XrdAccPrivs.hh
libXrdAcc_la_LIBADD = ../XrdOuc/libXrdOuc.la \
                      ../XrdNet/libXrdNet.la

XrdAccTest_SOURCES = XrdAccTest.cc XrdAccAuthorize.hh
XrdAccTest_LDADD = libXrdAcc.la ../XrdOuc/libXrdOuc.la ../XrdNet/libXrdNet.la
