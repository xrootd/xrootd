#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdPosix.la

# N.B.: XrdPosixXrootd.hh includes XrdSysPthread.hh!
xrootdposixincdir = $(includedir)/xrootd/XrdPosix
xrootdposixinc_HEADERS = XrdPosixXrootd.hh XrdPosixLinkage.hh \
                        XrdPosixExtern.hh  XrdPosixStream.hh

INCLUDES = -I$(top_srcdir)/src

libXrdPosix_la_SOURCES = \
        XrdPosix.cc  \
        XrdPosixLinkage.cc \
        XrdPosixXrootd.cc

libXrdPosix_la_LIBADD = ../XrdClient/libXrdClient.la \
                        ../XrdNet/libXrdNet.la \
                        ../XrdOuc/libXrdOuc.la
