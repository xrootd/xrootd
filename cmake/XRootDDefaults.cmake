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
option( ENABLE_FUSE      "Enable the fuse filesystem driver if possible."                 TRUE )
option( ENABLE_CRYPTO    "Enable the OpenSSL cryprography support."                       TRUE )
option( ENABLE_KRB5      "Enable the Kerberos 5 authentication if possible."              TRUE )
option( ENABLE_READLINE  "Enable the lib readline support in the commandline utilities."  TRUE )
option( ENABLE_XRDCL     "Enable XRootD client."                                          TRUE )
option( ENABLE_TESTS     "Enable unit tests."                                             FALSE )
option( ENABLE_HTTP      "Enable HTTP component."                                         TRUE )
option( ENABLE_PYTHON    "Enable python bindings."                                        TRUE )
option( XRDCL_ONLY       "Build only the client and necessary dependencies"               FALSE )
option( XRDCL_LIB_ONLY   "Build only the client libraries and necessary dependencies"     FALSE )
option( PYPI_BUILD       "The project is being built for PyPI release"                    FALSE )
define_default( XRD_PYTHON_REQ_VERSION 2.4 )
