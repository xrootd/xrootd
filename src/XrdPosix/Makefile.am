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

# N.B.: XrdPosixXrootd.hh includes XrdOucPthread.hh!
xrootdposixincdir = $(includedir)/xrootd/XrdPosix
xrootdposixinc_HEADERS = XrdPosixXrootd.hh

INCLUDES = -I$(top_srcdir)/src

libXrdPosix_la_SOURCES = \
        XrdPosix.cc  \
        XrdPosixXrootd.cc XrdPosixXrootd.hh

libXrdPosix_la_LIBADD = ../XrdClient/libXrdClient.la \
                        ../XrdSec/libXrdSec.la \
                        ../XrdNet/libXrdNet.la \
                        ../XrdOuc/libXrdOuc.la
