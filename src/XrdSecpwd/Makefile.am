#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdSecpwd.la

bin_PROGRAMS = xrdpwdadmin

INCLUDES = -I$(top_srcdir)/src

libXrdSecpwd_la_SOURCES = \
        XrdSecProtocolpwd.cc XrdSecProtocolpwd.hh XrdSecpwdPlatform.hh

xrdpwdadmin_SOURCES = XrdSecpwdSrvAdmin.cc
xrdpwdadmin_LDADD =   ../XrdCrypto/libXrdCrypto.la \
                      ../XrdNet/libXrdNet.la \
                      ../XrdOuc/libXrdOuc.la \
		      $(OPENSSL_LIBDIR) -ldl -lcrypto
xrdpwdadmin_LDFLAGS = -static

