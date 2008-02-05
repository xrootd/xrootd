#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdPosix.la libXrdPosixPreload.la

# N.B.: XrdPosixXrootd.hh includes XrdSysPthread.hh!
xrootdposixincdir = $(includedir)/xrootd/XrdPosix
xrootdposixinc_HEADERS = XrdPosixXrootd.hh XrdPosixLinkage.hh \
                        XrdPosix.hh XrdPosixExtern.hh


INCLUDES = -I$(top_srcdir)/src

libXrdPosix_la_SOURCES = \
        XrdPosix.cc  \
        XrdPosixLinkage.cc \
        XrdPosixXrootd.cc 

libXrdPosix_la_LIBADD = ../XrdClient/libXrdClient.la \
                        ../XrdSec/libXrdSec.la \
                        ../XrdNet/libXrdNet.la \
                        ../XrdOuc/libXrdOuc.la \
                        ../XrdSys/libXrdSys.la

libXrdPosixPreload_la_SOURCES = \
         XrdPosixPreload.cc  \
         XrdPosixLinkage.cc \
         XrdPosixXrootd.cc \
         XrdPosix.cc

# TODO: What to do with XrdPosixPreload32.cc?

libXrdPosixPreload_la_LIBADD = ../XrdClient/libXrdClient.la \
                         ../XrdNet/libXrdNet.la \
                         ../XrdOuc/libXrdOuc.la \
                         ../XrdSec/libXrdSec.la \
                         ../XrdSys/libXrdSys.la

