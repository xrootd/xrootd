
# This is to avoid linking XrdUtils against all of libXrdCryptossl

target_sources(XrdUtils PRIVATE
  XrdCryptoAux.cc         XrdCryptoAux.hh
  XrdCryptoRSA.cc         XrdCryptoRSA.hh
                          XrdCryptoTrace.hh
  XrdCryptoX509.cc        XrdCryptoX509.hh
  XrdCryptoX509Chain.cc   XrdCryptoX509Chain.hh
  XrdCryptoX509Crl.cc     XrdCryptoX509Crl.hh
  XrdCryptoX509Req.cc     XrdCryptoX509Req.hh
  XrdCryptosslAux.cc      XrdCryptosslAux.hh
  XrdCryptosslRSA.cc      XrdCryptosslRSA.hh
  XrdCryptosslX509.cc     XrdCryptosslX509.hh
  XrdCryptosslX509Crl.cc  XrdCryptosslX509Crl.hh
  XrdCryptosslX509Req.cc  XrdCryptosslX509Req.hh
  XrdCryptosslgsiAux.cc
)

add_library(XrdCryptoLite SHARED
  XrdCryptoLite.cc  XrdCryptoLite.hh
  XrdCryptoLite_bf32.cc
)

target_link_libraries(XrdCryptoLite PRIVATE XrdUtils OpenSSL::Crypto)
set_target_properties(XrdCryptoLite PROPERTIES VERSION 2.0.0 SOVERSION 2)

add_library(XrdCrypto SHARED
  XrdCryptoAux.cc           XrdCryptoAux.hh
  XrdCryptoBasic.cc         XrdCryptoBasic.hh
  XrdCryptoCipher.cc        XrdCryptoCipher.hh
  XrdCryptoFactory.cc       XrdCryptoFactory.hh
  XrdCryptoMsgDigest.cc     XrdCryptoMsgDigest.hh
  XrdCryptoRSA.cc           XrdCryptoRSA.hh
                            XrdCryptoTrace.hh
  XrdCryptoX509.cc          XrdCryptoX509.hh
  XrdCryptoX509Chain.cc     XrdCryptoX509Chain.hh
  XrdCryptoX509Crl.cc       XrdCryptoX509Crl.hh
  XrdCryptoX509Req.cc       XrdCryptoX509Req.hh
  XrdCryptogsiX509Chain.cc  XrdCryptogsiX509Chain.hh
)

target_link_libraries(XrdCrypto PRIVATE XrdUtils ${CMAKE_DL_LIBS})
set_target_properties(XrdCrypto PROPERTIES VERSION 2.0.0 SOVERSION 2)

set(XrdCryptossl XrdCryptossl-${PLUGIN_VERSION})

add_library(${XrdCryptossl} MODULE
  XrdCryptosslAux.cc       XrdCryptosslAux.hh
  XrdCryptosslCipher.cc    XrdCryptosslCipher.hh
  XrdCryptosslFactory.cc   XrdCryptosslFactory.hh
  XrdCryptosslMsgDigest.cc XrdCryptosslMsgDigest.hh
  XrdCryptosslRSA.cc       XrdCryptosslRSA.hh
                           XrdCryptosslTrace.hh
  XrdCryptosslX509.cc      XrdCryptosslX509.hh
  XrdCryptosslX509Crl.cc   XrdCryptosslX509Crl.hh
  XrdCryptosslX509Req.cc   XrdCryptosslX509Req.hh
  XrdCryptosslgsiAux.cc
)

target_link_libraries(${XrdCryptossl}
  PRIVATE
    XrdCrypto
    XrdUtils
    OpenSSL::SSL
    ${CMAKE_THREAD_LIBS_INIT}
)

add_dependencies(plugins ${XrdCryptossl})

install(
  TARGETS
    XrdCrypto
    XrdCryptoLite
    ${XrdCryptossl}
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
