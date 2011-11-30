#-------------------------------------------------------------------------------
# Define the default build parameters
#-------------------------------------------------------------------------------

if( "${CMAKE_BUILD_TYPE}" STREQUAL "" )
  if( Solaris AND NOT SUNCC_CAN_DO_OPTS )
    set( CMAKE_BUILD_TYPE Debug )
  else()
    set( CMAKE_BUILD_TYPE RelWithDebInfo )
  endif()
endif()

if( Solaris )
  define_default( ENABLE_PERL     FALSE )
else()
  define_default( ENABLE_PERL     TRUE )
endif()

define_default( ENABLE_FUSE     TRUE )
define_default( ENABLE_CRYPTO   TRUE )
define_default( ENABLE_KRB5     TRUE )
define_default( ENABLE_READLINE TRUE )

if( Linux OR MacOSX )
  define_default( ENABLE_BONJOUR  TRUE )
else()
  define_default( ENABLE_BONJOUR  FALSE )
endif()
