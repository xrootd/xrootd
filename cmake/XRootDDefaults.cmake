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

define_default( PLUGIN_VERSION  4 )
define_default( ENABLE_TESTS    FALSE )
define_default( ENABLE_CEPH     TRUE )
