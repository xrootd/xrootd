#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

noinst_HEADERS = XrdOss.hh

noinst_LTLIBRARIES = libXrdOss.la

INCLUDES = -I$(top_srcdir)/src

libXrdOss_la_SOURCES = \
        XrdOssAio.cc XrdOssTrace.hh \
        XrdOssApi.cc XrdOssApi.hh XrdOssError.hh \
        XrdOssCache.cc XrdOssCache.hh \
        XrdOssConfig.cc XrdOssConfig.hh \
        XrdOssCreate.cc XrdOssOpaque.hh \
        XrdOssLock.cc XrdOssLock.hh   \
        XrdOssMio.cc XrdOssMio.hh XrdOssMioFile.hh   \
        XrdOssMSS.cc    \
        XrdOssRename.cc \
        XrdOssStage.cc  \
        XrdOssUnlink.cc
libXrdOss_la_LIBADD = $(LIBRT)
