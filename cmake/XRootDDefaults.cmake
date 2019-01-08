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

define_default( PLUGIN_VERSION    4 )
define_default( ENABLE_FUSE       TRUE )
define_default( ENABLE_CRYPTO     TRUE )
define_default( ENABLE_KRB5       TRUE )
define_default( ENABLE_READLINE   TRUE )
define_default( ENABLE_XRDCL      TRUE )
define_default( ENABLE_TESTS      FALSE )
define_default( ENABLE_HTTP       TRUE )
define_default( ENABLE_CEPH       TRUE )
define_default( ENABLE_PYTHON     TRUE )
define_default( XRD_PYTHON_REQ_VERSION 2.4 )
