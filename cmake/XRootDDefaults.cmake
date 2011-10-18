#-------------------------------------------------------------------------------
# Define the default build parameters
#-------------------------------------------------------------------------------

if( Solaris )
  define_default( ENABLE_PERL     FALSE )
else()
  define_default( ENABLE_PERL     TRUE )
endif()

define_default( ENABLE_FUSE     TRUE )
define_default( ENABLE_CRYPTO   TRUE )
define_default( ENABLE_KRB5     TRUE )
define_default( ENABLE_READLINE TRUE )
