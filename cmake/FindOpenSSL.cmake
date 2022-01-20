include( FindPackageHandleStandardArgs )

set( OPENSSL_GOOD_VERSION TRUE )
if( OPENSSL_INCLUDE_DIR AND OPENSSL_LIBRARIES )
  set( OPENSSL_FOUND TRUE )
else()
  find_path(
    OPENSSL_INCLUDE_DIR
    NAMES openssl/ssl.h
    HINTS
    ${OPENSSL_ROOT_DIR}
    PATH_SUFFIXES
    include )

  file(STRINGS ${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h opensslvers REGEX "^# define OPENSSL_VERSION_NUMBER")
  string(REGEX REPLACE "# define[ ]+OPENSSL_VERSION_NUMBER[ ]+" "" opensslvers ${opensslvers})

  set( LIBSSLNAME ssl )
  set( LIBCRYPTONAME crypto )

  if ( OPENSSL_GOOD_VERSION )

    find_library(
      OPENSSL_SSL_LIBRARY
      NAMES ${LIBSSLNAME}
      HINTS
      ${OPENSSL_ROOT_DIR}
      PATH_SUFFIXES
      ${LIBRARY_PATH_PREFIX}
      ${LIB_SEARCH_OPTIONS})

    string(FIND ${OPENSSL_SSL_LIBRARY} ".a" hasstaticext)
    if (NOT ${hasstaticext} EQUAL -1)
      if ( ${opensslvers} LESS "0x1000201fL" )
        set( OPENSSL_GOOD_VERSION FALSE )
        message(WARNING " >>> Cannot build XRootD crypto modules: static openssl build version is < 1.0.2")
      else()
        set(OPENSSL_USE_STATIC TRUE)
        set( LIBCRYPTONAME libcrypto.a )
      endif()
      message("-- Using OpenSSL static libraries (version: " ${opensslvers} ")")
    endif()

  endif()

  if ( OPENSSL_GOOD_VERSION )
    find_library(
      OPENSSL_CRYPTO_LIBRARY
      NAMES ${LIBCRYPTONAME}
      HINTS
      ${OPENSSL_ROOT_DIR}
      PATH_SUFFIXES
      ${LIBRARY_PATH_PREFIX}
      ${LIB_SEARCH_OPTIONS})

    set( OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} )

    find_package_handle_standard_args(
      OpenSSL
      DEFAULT_MSG
      OPENSSL_LIBRARIES)

    mark_as_advanced( OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES )

  endif()
endif()


#-------------------------------------------------------------------------------
# Check for the TLS support in the OpenSSL version that is available
# (assume available if use of static libs is detected and the openssl version
# is at least 1.0.2)
#-------------------------------------------------------------------------------

if ( OPENSSL_FOUND )

  if( OPENSSL_USE_STATIC AND OPENSSL_GOOD_VERSION )

    add_definitions( -DHAVE_TLS -DHAVE_TLS12 -DHAVE_TLS11 -DHAVE_TLS1 -DHAVE_DH_PADDED -DHAVE_DH_PADDED_FUNC )

  else()

    set( CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES} )
    set( CMAKE_REQUIRED_QUIET FALSE)

    check_function_exists(TLS_method HAVE_TLS_FUNC)
    check_symbol_exists(
            TLS_method
            ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h
            HAVE_TLS_SYMB)
    if( HAVE_TLS_FUNC AND HAVE_TLS_SYMB )
        add_definitions( -DHAVE_TLS )
    endif()

    check_function_exists(TLSv1_2_method HAVE_TLS12_FUNC)
    check_symbol_exists(
            TLSv1_2_method
            ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h
            HAVE_TLS12_SYMB)
    if( HAVE_TLS12_FUNC AND HAVE_TLS12_SYMB )
        add_definitions( -DHAVE_TLS12 )
    endif()

    check_function_exists(TLSv1_1_method HAVE_TLS11_FUNC)
    check_symbol_exists(
            TLSv1_1_method
            ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h
            HAVE_TLS11_SYMB)
    if( HAVE_TLS11_FUNC AND HAVE_TLS11_SYMB )
        add_definitions( -DHAVE_TLS11 )
    endif()

    check_function_exists(TLSv1_method HAVE_TLS1_FUNC)
    check_symbol_exists(
            TLSv1_method
            ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h
            HAVE_TLS1_SYMB)
    if( HAVE_TLS1_FUNC AND HAVE_TLS1_SYMB )
        add_definitions( -DHAVE_TLS1 )
    endif()

    check_function_exists(DH_compute_key_padded HAVE_DH_PADDED_FUNC)
    check_symbol_exists(
            DH_compute_key_padded
            ${OPENSSL_INCLUDE_DIR}/openssl/dh.h
        HAVE_DH_PADDED_SYMB)
    if( HAVE_DH_PADDED_FUNC)
       if( HAVE_DH_PADDED_SYMB )
         add_definitions( -DHAVE_DH_PADDED )
       else()
         add_definitions( -DHAVE_DH_PADDED_FUNC )
       endif()
    endif()

  endif()
endif()
