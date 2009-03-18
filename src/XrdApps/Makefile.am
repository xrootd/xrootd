#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 17.3.2009
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

#############################################################################
## extra directory definitions
xrootdappsincdir = $(includedir)/xrootd/XrdApps

#############################################################################
## deliverables

bin_PROGRAMS = xrdadler32

xrootdappsinc_HEADERS =


#############################################################################
## Build rules

AM_CPPFLAGS = -I$(top_srcdir)/src

libXrdApps_la_SOURCES = \
                        Xrdadler32.cc

libXrdApps_la_LIBADD = \
                       ../XrdOss/libXrdClient.la  \
                       ../XrdNet/libXrdPosix.la   \
                       ../XrdOuc/libXrdOuc.la     \
                       ../XrdSys/libXrdSys.la

xrdadler32_LDFLAGS = -static -export-dynamic -ldl -lz
