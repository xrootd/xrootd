#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdOfs.la

INCLUDES = -I$(top_srcdir)/src

libXrdOfs_la_SOURCES = \
        XrdOfs.cc XrdOfs.hh XrdOfsSecurity.hh XrdOfsTrace.hh XrdOfs.icc \
        XrdOfsConfig.cc XrdOfsConfig.hh \
        XrdOfsEvr.cc XrdOfsEvr.hh   \
        XrdOfsEvs.cc XrdOfsEvs.hh   \
        XrdOfsHandle.cc XrdOfsHandle.hh

libXrdOfs_la_LIBADD = ../XrdAcc/libXrdAcc.la \
                      ../XrdOss/libXrdOss.la \
                      ../XrdOdc/libXrdOdc.la \
                      ../XrdNet/libXrdNet.la \
                      ../XrdOuc/libXrdOuc.la
