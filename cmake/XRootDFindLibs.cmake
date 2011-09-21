#-------------------------------------------------------------------------------
# Find the required libraries
#-------------------------------------------------------------------------------
find_package( Readline )
find_package( ZLIB REQUIRED)
find_package( OpenSSL )
find_package( Kerberos5 )
find_package( fuse )
find_package( PerlLibs )
find_package( SWIG )

if( ENABLE_READLINE AND READLINE_FOUND )
  add_definitions( -DHAVE_READLINE )
else()
  set( READLINE_LIBRARY "" )
  set( NCURSES_LIBRARY "" )
endif()

if( ZLIB_FOUND )
  add_definitions( -DHAVE_LIBZ )
endif()

if( ENABLE_CRYPTO AND OPENSSL_FOUND )
  add_definitions( -DHAVE_XRDCRYPTO )
  set( BUILD_CRYPTO TRUE )
else()
  set( BUILD_CRYPTO FALSE )
endif()

if( ENABLE_KRB5 AND KERBEROS5_FOUND )
  set( BUILD_KRB5 TRUE )
else()
  set( BUILD_KRB5 FALSE )
endif()

if( ENABLE_FUSE AND FUSE_FOUND )
  add_definitions( -DHAVE_FUSE )
  set( BUILD_FUSE TRUE )
else()
  set( BUILD_FUSE FALSE )
endif()

if( ENABLE_PERL AND PERLLIBS_FOUND )
  set( BUILD_PERL TRUE )
else()
  set( BUILD_PERL FALSE )
endif()
