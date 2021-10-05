include( XRootDCommon )

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

if( WITH_OPENSSL3 )
  set( XrdCryptoSources
    XrdCrypto/XrdCryptoAux.cc                 XrdCrypto/XrdCryptoAux.hh
    XrdCrypto/XrdCryptoTrace.hh
    XrdCrypto/XrdCryptoBasic.cc               XrdCrypto/XrdCryptoBasic.hh
    XrdCrypto/XrdCryptoCipher.cc              XrdCrypto/XrdCryptoCipher.hh
    XrdCrypto/XrdCryptoFactory.cc             XrdCrypto/XrdCryptoFactory.hh
    XrdCrypto/XrdCryptoMsgDigest.cc           XrdCrypto/XrdCryptoMsgDigest.hh
    XrdCrypto/XrdCryptoRSA.cc                 XrdCrypto/XrdCryptoRSA.hh
    XrdCrypto/XrdCryptoX509.cc                XrdCrypto/XrdCryptoX509.hh
    XrdCrypto/openssl3/XrdCryptoX509Chain.cc  XrdCrypto/XrdCryptoX509Chain.hh
    XrdCrypto/XrdCryptoX509Crl.cc             XrdCrypto/XrdCryptoX509Crl.hh
    XrdCrypto/XrdCryptoX509Req.cc             XrdCrypto/XrdCryptoX509Req.hh
    XrdCrypto/XrdCryptogsiX509Chain.cc        XrdCrypto/XrdCryptogsiX509Chain.hh )
else()
  set( XrdCryptoSources
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
    XrdCrypto/XrdCryptogsiX509Chain.cc      XrdCrypto/XrdCryptogsiX509Chain.hh )
endif()

#-------------------------------------------------------------------------------
# The XrdCrypto library
#-------------------------------------------------------------------------------
add_library(
  XrdCrypto
  SHARED
  ${XrdCryptoSources} )

target_link_libraries(
  XrdCrypto
  XrdUtils
  ${CMAKE_DL_LIBS} )

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

if( WITH_OPENSSL3 )
  set( XrdCryptoLiteSources
       XrdCrypto/XrdCryptoLite.cc   XrdCrypto/XrdCryptoLite.hh
       XrdCrypto/openssl3/XrdCryptoLite_bf32.cc )
else()
  set( XrdCryptoLiteSources
       XrdCrypto/XrdCryptoLite.cc   XrdCrypto/XrdCryptoLite.hh
       XrdCrypto/XrdCryptoLite_bf32.cc )
endif()

add_library(
  XrdCryptoLite
  SHARED
  ${XrdCryptoLiteSources} )

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
# The XrdCryptossl module
#-------------------------------------------------------------------------------
if( BUILD_CRYPTO )
  include_directories( ${OPENSSL_INCLUDE_DIR} )

  if( WITH_OPENSSL3 )
    set( XrdCryptosslSources 
         XrdCrypto/openssl3/XrdCryptosslAux.cc       XrdCrypto/XrdCryptosslAux.hh
         XrdCrypto/openssl3/XrdCryptosslgsiAux.cc
         XrdCrypto/openssl3/XrdCryptosslCipher.cc    XrdCrypto/openssl3/XrdCryptosslCipher.hh
         XrdCrypto/XrdCryptosslMsgDigest.cc          XrdCrypto/XrdCryptosslMsgDigest.hh
         XrdCrypto/openssl3/XrdCryptosslRSA.cc       XrdCrypto/XrdCryptosslRSA.hh
         XrdCrypto/openssl3/XrdCryptosslX509.cc      XrdCrypto/XrdCryptosslX509.hh
         XrdCrypto/XrdCryptosslX509Crl.cc            XrdCrypto/XrdCryptosslX509Crl.hh
         XrdCrypto/XrdCryptosslX509Req.cc            XrdCrypto/XrdCryptosslX509Req.hh
         XrdCrypto/XrdCryptosslTrace.hh
         XrdCrypto/openssl3/XrdCryptosslFactory.cc   XrdCrypto/XrdCryptosslFactory.hh )
  else()
    set( XrdCryptosslSources
         XrdCrypto/XrdCryptosslAux.cc            XrdCrypto/XrdCryptosslAux.hh
         XrdCrypto/XrdCryptosslgsiAux.cc
         XrdCrypto/XrdCryptosslCipher.cc         XrdCrypto/XrdCryptosslCipher.hh
         XrdCrypto/XrdCryptosslMsgDigest.cc      XrdCrypto/XrdCryptosslMsgDigest.hh
         XrdCrypto/XrdCryptosslRSA.cc            XrdCrypto/XrdCryptosslRSA.hh
         XrdCrypto/XrdCryptosslX509.cc           XrdCrypto/XrdCryptosslX509.hh
         XrdCrypto/XrdCryptosslX509Crl.cc        XrdCrypto/XrdCryptosslX509Crl.hh
         XrdCrypto/XrdCryptosslX509Req.cc        XrdCrypto/XrdCryptosslX509Req.hh
         XrdCrypto/XrdCryptosslTrace.hh
         XrdCrypto/XrdCryptosslFactory.cc        XrdCrypto/XrdCryptosslFactory.hh )
  endif()

  add_library(
    ${LIB_XRD_CRYPTOSSL}
    MODULE
    ${XrdCryptosslSources} )

  target_link_libraries(
    ${LIB_XRD_CRYPTOSSL}
    XrdCrypto
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES} )

  set_target_properties(
    ${LIB_XRD_CRYPTOSSL}
    PROPERTIES
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
    TARGETS ${LIB_XRD_CRYPTOSSL}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
endif()
# FIXME: Unused files
#-rw-r--r-- 1 ljanyst ljanyst 16721 2011-03-21 16:13 XrdCryptotest.cc


