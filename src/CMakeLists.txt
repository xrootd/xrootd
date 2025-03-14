#-------------------------------------------------------------------------------
# If this is a build for pip set up the rpath
# (this is especially important for plugins)
#-------------------------------------------------------------------------------
if( PYPI_BUILD )
  set( CMAKE_INSTALL_RPATH "$ORIGIN" )
  set( CMAKE_INSTALL_RPATH_USE_LINK_PATH true )
endif()

#-------------------------------------------------------------------------------
# XRDCL_LIB_ONLY implies XRDCL_ONLY
#-------------------------------------------------------------------------------
if( XRDCL_LIB_ONLY )
  set ( XRDCL_ONLY TRUE )
endif()

#-------------------------------------------------------------------------------
# Include the subcomponents
#-------------------------------------------------------------------------------
include( XrdUtils )
include( XrdHeaders )

add_subdirectory( XrdApps )
add_subdirectory( XrdCrypto )
add_subdirectory( XrdPosix )

add_subdirectory( XrdSec )
add_subdirectory( XrdSecgsi )
add_subdirectory( XrdSeckrb5 )
add_subdirectory( XrdSecpwd )
add_subdirectory( XrdSecsss )
add_subdirectory( XrdSecunix )
add_subdirectory( XrdSecztn )

add_subdirectory( XrdXml )

add_subdirectory( XrdCl )
add_subdirectory( XrdClHttp )
add_subdirectory( XrdEc )

if( NOT XRDCL_ONLY )
  include( XrdServer )
  include( XrdDaemons )
  include( XrdFrm )
  include( XrdFfs )
  include( XrdPlugins )
  include( XrdSsi )
  include( XrdPfc )
  add_subdirectory( XrdOssCsi )

  add_subdirectory( XrdHttp )
  add_subdirectory( XrdHttpTpc )

  add_subdirectory( XrdMacaroons )
  add_subdirectory( XrdVoms )
  add_subdirectory( XrdCeph )
  add_subdirectory( XrdSciTokens )
endif()
