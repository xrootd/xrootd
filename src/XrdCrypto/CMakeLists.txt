
#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_CRYPTOSSL     XrdCryptossl-${PLUGIN_VERSION} )
add_dependencies(plugins ${LIB_XRD_CRYPTOSSL})

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CRYPTO_VERSION   2.0.0 )
set( XRD_CRYPTO_SOVERSION 2 )

set( XRD_CRYPTO_LITE_VERSION   2.0.0 )
set( XRD_CRYPTO_LITE_SOVERSION 2 )

set( XrdCryptoSources
     XrdCryptoAux.cc               XrdCryptoAux.hh
     XrdCryptoTrace.hh
     XrdCryptoBasic.cc             XrdCryptoBasic.hh
     XrdCryptoCipher.cc            XrdCryptoCipher.hh
     XrdCryptoFactory.cc           XrdCryptoFactory.hh
     XrdCryptoMsgDigest.cc         XrdCryptoMsgDigest.hh
     XrdCryptoRSA.cc               XrdCryptoRSA.hh
     XrdCryptoX509.cc              XrdCryptoX509.hh
     XrdCryptoX509Chain.cc         XrdCryptoX509Chain.hh
     XrdCryptoX509Crl.cc           XrdCryptoX509Crl.hh
     XrdCryptoX509Req.cc           XrdCryptoX509Req.hh
     XrdCryptogsiX509Chain.cc      XrdCryptogsiX509Chain.hh )

#-------------------------------------------------------------------------------
# The XrdCrypto library
#-------------------------------------------------------------------------------
add_library(
  XrdCrypto
  SHARED
  ${XrdCryptoSources} )

target_link_libraries(
  XrdCrypto
  PRIVATE
  XrdUtils
  ${CMAKE_DL_LIBS} )

set_target_properties(
  XrdCrypto
  PROPERTIES
  VERSION   ${XRD_CRYPTO_VERSION}
  SOVERSION ${XRD_CRYPTO_SOVERSION} )

#-------------------------------------------------------------------------------
# The XrdCryptoLite library
#-------------------------------------------------------------------------------

set( XrdCryptoLiteSources
     XrdCryptoLite.cc      XrdCryptoLite.hh
     XrdCryptoLite_bf32.cc )

add_library(
  XrdCryptoLite
  SHARED
  ${XrdCryptoLiteSources} )

target_link_libraries(
  XrdCryptoLite
  PRIVATE
  XrdUtils
  OpenSSL::Crypto )

set_target_properties(
  XrdCryptoLite
  PROPERTIES
  VERSION   ${XRD_CRYPTO_LITE_VERSION}
  SOVERSION ${XRD_CRYPTO_LITE_SOVERSION} )

#-------------------------------------------------------------------------------
# The XrdCryptossl module
#-------------------------------------------------------------------------------

set( XrdCryptosslSources
     XrdCryptosslAux.cc       XrdCryptosslAux.hh
     XrdCryptosslgsiAux.cc
     XrdCryptosslCipher.cc    XrdCryptosslCipher.hh
     XrdCryptosslMsgDigest.cc XrdCryptosslMsgDigest.hh
     XrdCryptosslRSA.cc       XrdCryptosslRSA.hh
     XrdCryptosslX509.cc      XrdCryptosslX509.hh
     XrdCryptosslX509Crl.cc   XrdCryptosslX509Crl.hh
     XrdCryptosslX509Req.cc   XrdCryptosslX509Req.hh
     XrdCryptosslTrace.hh
     XrdCryptosslFactory.cc   XrdCryptosslFactory.hh )

add_library(
  ${LIB_XRD_CRYPTOSSL}
  MODULE
  ${XrdCryptosslSources} )

target_link_libraries(
  ${LIB_XRD_CRYPTOSSL}
  PRIVATE
  XrdCrypto
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  OpenSSL::SSL )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCrypto XrdCryptoLite
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  TARGETS ${LIB_XRD_CRYPTOSSL}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
# FIXME: Unused files
#-rw-r--r-- 1 ljanyst ljanyst 16721 2011-03-21 16:13 XrdCryptotest.cc
