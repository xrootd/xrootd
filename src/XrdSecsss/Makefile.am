#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 10.6.2008
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdSecsss.la

bin_PROGRAMS = xrdsssadmin

INCLUDES = -I$(top_srcdir)/src

libXrdSecsss_la_SOURCES = \
   XrdSecProtocolsss.cc XrdSecProtocolsss.hh  \
   XrdSecsssID.cc       XrdSecsssID.hh \
   XrdSecsssKT.cc       XrdSecsssKT.hh \
   XrdSecsssRR.hh
libXrdSecsss_la_LIBADD = ../XrdCrypto/libXrdCryptoLite.la
                      ../XrdOuc/libXrdOuc.la \
                      ../XrdOuc/libXrdNet.la \
                      ../XrdSys/libXrdSys.la \
		      $(CRYPTLIB) $(OPENSSL_LIBDIR) -lcrypto
libXrdSecsss_la_LDFLAGS = -module

xrdpwdadmin_SOURCES = XrdSecsssAdmin.cc
xrdpwdadmin_LDFLAGS = -static
xrdpwdadmin_LDADD =   ../XrdCrypto/libXrdCryptoLite.la \
                      ../XrdOuc/libXrdOuc.la \
                      ../XrdOuc/libXrdNet.la \
                      ../XrdSys/libXrdSys.la \
		      $(CRYPTLIB) $(OPENSSL_LIBDIR) -lcrypto
