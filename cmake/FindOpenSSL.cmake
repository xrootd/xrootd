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

check_function_exists(TLS_method HAVE_TLS)
compiler_define_if_found(HAVE_TLS HAVE_TLS)

check_function_exists(TLSv1_2_method HAVE_TLS12)
compiler_define_if_found(HAVE_TLS12 HAVE_TLS12)

check_function_exists(TLSv1_1_method HAVE_TLS11)
compiler_define_if_found(HAVE_TLS11 HAVE_TLS11)

check_function_exists(TLSv1_method HAVE_TLS1)
compiler_define_if_found(HAVE_TLS1 HAVE_TLS1)