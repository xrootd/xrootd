#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

xrootdbwmincdir = $(includedir)/xrootd/XrdBwm
xrootdbwminc_HEADERS = XrdBwm.hh \
                       XrdBwmHandle.hh \
                       XrdBwmLogger.hh \
                       XrdBwmPolicy.hh \
                       XrdBwmPolicy1.hh \
                       XrdBwmTrace.hh

INCLUDES = -I$(top_srcdir)/src

libXrdBwm_la_SOURCES = \
                       XrdBwm.cc \
                       XrdBwmConfig.cc \
                       XrdBwmHandle.cc \
                       XrdBwmLogger.cc \
                       XrdBwmPolicy1.cc

libXrdProxy_la_LIBADD = ../XrdAcc/libXrdAcc.la \
                        ../XrdNet/libXrdNet.la \
                        ../XrdOuc/libXrdOuc.la \
                        ../XrdSys/libXrdSys.la
