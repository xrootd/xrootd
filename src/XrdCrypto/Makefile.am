#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

## Note: These two files are not used in any part of the build. I still
## include them in the distribution via the EXTRA_DIST keyword
EXTRA_DIST = XrdCryptosslX509Store.cc XrdCryptosslX509Store.hh

lib_LTLIBRARIES = libXrdCrypto.la
noinst_LTLIBRARIES = libXrdSSLutil.la

check_PROGRAMS = xrdcryptotest

INCLUDES = -I$(top_srcdir)/src $(OPENSSL_INCDIR) $(KRB5_INCDIR)


libXrdSSLutil_la_SOURCES = \
        XrdCryptosslAux.cc XrdCryptosslAux.hh XrdCryptosslTrace.hh       \
        XrdCryptosslCipher.cc XrdCryptosslCipher.hh        \
        XrdCryptosslMsgDigest.cc XrdCryptosslMsgDigest.hh     \
        XrdCryptosslRSA.cc XrdCryptosslRSA.hh     \
        XrdCryptosslX509.cc XrdCryptosslX509.hh     \
        XrdCryptosslX509Crl.cc XrdCryptosslX509Crl.hh     \
        XrdCryptosslX509Req.cc XrdCryptosslX509Req.hh     \
        XrdCryptosslFactory.cc XrdCryptosslFactory.hh     \
        XrdCryptosslgsiAux.cc XrdCryptosslgsiAux.hh \
        XrdCryptosslgsiX509Chain.cc XrdCryptosslgsiX509Chain.hh
libXrdSSLutil_la_LIBADD = ../XrdSut/libXrdSut.la \
                          $(OPENSSL_LIBDIR) -lssl
libXrdSSLutil_la_CXXFLAGS = $(SSLCXXFLAGS)



libXrdCrypto_la_SOURCES = \
        PC1.cc PC1.hh    \
        PC3.cc PC3.hh    \
        XrdCryptoAux.cc XrdCryptoAux.hh XrdCryptoTrace.hh    \
        XrdCryptoBasic.cc XrdCryptoBasic.hh     \
        XrdCryptoCipher.cc XrdCryptoCipher.hh     \
        XrdCryptoFactory.cc XrdCryptoFactory.hh \
        XrdCryptoMsgDigest.cc XrdCryptoMsgDigest.hh \
        XrdCryptoRSA.cc XrdCryptoRSA.hh \
        XrdCryptoX509.cc XrdCryptoX509.hh \
        XrdCryptoX509Chain.cc XrdCryptoX509Chain.hh \
        XrdCryptoX509Crl.cc XrdCryptoX509Crl.hh \
        XrdCryptoX509Req.cc XrdCryptoX509Req.hh \
        XrdCryptolocalCipher.cc XrdCryptolocalCipher.hh \
        XrdCryptolocalFactory.cc XrdCryptolocalFactory.hh
libXrdCrypto_la_LIBADD = libXrdSSLutil.la \
                         ../XrdOuc/libXrdOuc.la \
                         ../XrdNet/libXrdNet.la


xrdcryptotest_SOURCES = XrdCryptotest.cc
xrdcryptotest_LDADD = libXrdCrypto.la \
                      $(OPENSSL_LIBDIR) -lcrypto
