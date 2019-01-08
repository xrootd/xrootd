include( FindPackageHandleStandardArgs )

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

  find_library(
    OPENSSL_SSL_LIBRARY
    NAMES ssl
    HINTS
    ${OPENSSL_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  find_library(
    OPENSSL_CRYPTO_LIBRARY
    NAMES crypto
    HINTS
    ${OPENSSL_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  set( OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} )

  find_package_handle_standard_args(
    OpenSSL
    DEFAULT_MSG
    OPENSSL_LIBRARIES OPENSSL_INCLUDE_DIR )

  mark_as_advanced( OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES )
endif()


#-------------------------------------------------------------------------------
# Check for the TLS support in the OpenSSL version that is available
#-------------------------------------------------------------------------------

set ( CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES} )

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
