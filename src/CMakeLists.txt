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
include( XrdApps )
include( XrdCrypto )
include( XrdPosix )
include( XrdSec )
include( XrdXml )
include( XrdHeaders )
include( XrdSecgsi )
include( XrdSecztn )

if( BUILD_KRB5 )
  include( XrdSeckrb5 )
endif()

if( BUILD_XRDEC )
  include( XrdIsal )
endif()

if( ENABLE_XRDCL )
  add_subdirectory( XrdCl )
endif()

if( BUILD_XRDCLHTTP )
  add_subdirectory( XrdClHttp )
endif()

if( BUILD_XRDEC )
  add_subdirectory( XrdEc )
endif()

if( NOT XRDCL_ONLY )
  include( XrdServer )
  include( XrdDaemons )
  include( XrdFrm )
  include( XrdFfs )
  include( XrdPlugins )
  include( XrdSsi )
  include( XrdPfc )
  include( XrdOssCsi )

  if( BUILD_HTTP )
    include( XrdHttp )
    include( XrdTpc )
  endif()

  if( BUILD_MACAROONS )
    include( XrdMacaroons )
  endif()

  if( BUILD_VOMS )
    include( XrdVoms )
  endif()

  if( XRDCEPH_SUBMODULE )
    add_subdirectory( XrdCeph )
  endif()

  if( BUILD_SCITOKENS )
    include( XrdSciTokens )
  endif()

endif()
