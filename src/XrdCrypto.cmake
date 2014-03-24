
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CRYPTO_VERSION   1.0.0 )
set( XRD_CRYPTO_SOVERSION 1 )

set( XRD_CRYPTO_LITE_VERSION   1.0.0 )
set( XRD_CRYPTO_LITE_SOVERSION 1 )

set( XRD_CRYPTO_SSL_VERSION   2.0.0 )
set( XRD_CRYPTO_SSL_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdCrypto library
#-------------------------------------------------------------------------------
add_library(
  XrdCrypto
  SHARED
  XrdCrypto/PC1.cc                        XrdCrypto/PC1.hh
  XrdCrypto/PC3.cc                        XrdCrypto/PC3.hh
  XrdCrypto/XrdCryptoAux.cc               XrdCrypto/XrdCryptoAux.hh
  XrdCrypto/XrdCryptoTrace.hh
  XrdCrypto/XrdCryptoBasic.cc             XrdCrypto/XrdCryptoBasic.hh
  XrdCrypto/XrdCryptoCipher.cc            XrdCrypto/XrdCryptoCipher.hh
  XrdCrypto/XrdCryptoFactory.cc           XrdCrypto/XrdCryptoFactory.hh
  XrdCrypto/XrdCryptoMsgDigest.cc         XrdCrypto/XrdCryptoMsgDigest.hh
  XrdCrypto/XrdCryptoRSA.cc               XrdCrypto/XrdCryptoRSA.hh
  XrdCrypto/XrdCryptoX509.cc              XrdCrypto/XrdCryptoX509.hh
  XrdCrypto/XrdCryptoX509Chain.cc         XrdCrypto/XrdCryptoX509Chain.hh
  XrdCrypto/XrdCryptoX509Crl.cc           XrdCrypto/XrdCryptoX509Crl.hh
  XrdCrypto/XrdCryptoX509Req.cc           XrdCrypto/XrdCryptoX509Req.hh
  XrdCrypto/XrdCryptolocalCipher.cc       XrdCrypto/XrdCryptolocalCipher.hh
  XrdCrypto/XrdCryptolocalFactory.cc      XrdCrypto/XrdCryptolocalFactory.hh )

target_link_libraries(
  XrdCrypto
  XrdUtils
  dl )

set_target_properties(
  XrdCrypto
  PROPERTIES
  VERSION   ${XRD_CRYPTO_VERSION}
  SOVERSION ${XRD_CRYPTO_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdCryptoLite library
#-------------------------------------------------------------------------------
add_library(
  XrdCryptoLite
  SHARED
  XrdCrypto/XrdCryptoLite.cc              XrdCrypto/XrdCryptoLite.hh
  XrdCrypto/XrdCryptoLite_bf32.cc )

if( BUILD_CRYPTO )
  target_link_libraries(
    XrdCryptoLite
    XrdUtils
    ${OPENSSL_CRYPTO_LIBRARY} )
else()
  target_link_libraries(
    XrdCryptoLite
    XrdUtils )
endif()

set_target_properties(
  XrdCryptoLite
  PROPERTIES
  VERSION   ${XRD_CRYPTO_LITE_VERSION}
  SOVERSION ${XRD_CRYPTO_LITE_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdCryptossl library
#-------------------------------------------------------------------------------
if( BUILD_CRYPTO )
  include_directories( ${OPENSSL_INCLUDE_DIR} )

  add_library(
    XrdCryptossl
    SHARED
    XrdCrypto/XrdCryptosslAux.cc            XrdCrypto/XrdCryptosslAux.hh
    XrdCrypto/XrdCryptosslCipher.cc         XrdCrypto/XrdCryptosslCipher.hh
    XrdCrypto/XrdCryptosslFactory.cc        XrdCrypto/XrdCryptosslFactory.hh
    XrdCrypto/XrdCryptosslMsgDigest.cc      XrdCrypto/XrdCryptosslMsgDigest.hh
    XrdCrypto/XrdCryptosslRSA.cc            XrdCrypto/XrdCryptosslRSA.hh
    XrdCrypto/XrdCryptosslX509.cc           XrdCrypto/XrdCryptosslX509.hh
    XrdCrypto/XrdCryptosslX509Crl.cc        XrdCrypto/XrdCryptosslX509Crl.hh
    XrdCrypto/XrdCryptosslX509Req.cc        XrdCrypto/XrdCryptosslX509Req.hh
    XrdCrypto/XrdCryptosslgsiAux.cc         XrdCrypto/XrdCryptosslgsiAux.hh
    XrdCrypto/XrdCryptosslgsiX509Chain.cc   XrdCrypto/XrdCryptosslgsiX509Chain.hh
    XrdCrypto/XrdCryptosslTrace.hh )

  target_link_libraries(
    XrdCryptossl
    XrdCrypto
    XrdUtils
    pthread
    ${OPENSSL_LIBRARIES} )

  set_target_properties(
    XrdCryptossl
    PROPERTIES
    VERSION   ${XRD_CRYPTO_SSL_VERSION}
    SOVERSION ${XRD_CRYPTO_SSL_SOVERSION}
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES "" )
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCrypto XrdCryptoLite
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

if( BUILD_CRYPTO )
  install(
    TARGETS XrdCryptossl
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
endif()
# FIXME: Unused files
#-rw-r--r-- 1 ljanyst ljanyst  2499 2011-03-21 16:13 XrdCryptosslX509Store.cc
#-rw-r--r-- 1 ljanyst ljanyst  1750 2011-03-21 16:13 XrdCryptosslX509Store.hh
#-rw-r--r-- 1 ljanyst ljanyst 16721 2011-03-21 16:13 XrdCryptotest.cc


