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
include( XrdSec )
include( XrdXml )
include( XrdHeaders )

if( BUILD_CRYPTO )
  include( XrdSecgsi )
endif()

if( BUILD_KRB5 )
  include( XrdSeckrb5 )
endif()

if( BUILD_XRDEC )
  include( XrdIsal )
endif()

if( ENABLE_XRDCL )
  add_subdirectory( XrdCl )
endif()

if( XRDCLHTTP_SUBMODULE )
  add_subdirectory( XrdClHttp )
endif()

if( BUILD_XRDEC )
  add_subdirectory( XrdEc )
endif()

if( NOT XRDCL_ONLY )
  include( XrdServer )
  include( XrdDaemons )
  include( XrdFrm )
  include( XrdPosix )
  include( XrdFfs )
  include( XrdPlugins )
  include( XrdSsi )

  include( XrdPfc )

  if( CMAKE_COMPILER_IS_GNUCXX )
    include( XrdOssCsi )
  endif()

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
    include( XrdSecztn )
  endif()

endif()

#-------------------------------------------------------------------------------
# Install the utility scripts
#-------------------------------------------------------------------------------
if( NOT XRDCL_ONLY )
install(
  FILES
  ${CMAKE_SOURCE_DIR}/utils/XrdCmsNotify.pm
  ${CMAKE_SOURCE_DIR}/utils/netchk
  ${CMAKE_SOURCE_DIR}/utils/XrdOlbMonPerf
  ${CMAKE_SOURCE_DIR}/utils/cms_monPerf
  DESTINATION ${CMAKE_INSTALL_DATADIR}/xrootd/utils
  PERMISSIONS
  OWNER_EXECUTE OWNER_WRITE OWNER_READ
  GROUP_EXECUTE GROUP_READ
  WORLD_EXECUTE WORLD_READ )
endif()

if( NOT XRDCL_LIB_ONLY )
#-------------------------------------------------------------------------------
# Install xrootd-config
#-------------------------------------------------------------------------------
install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND cat ${CMAKE_SOURCE_DIR}/utils/xrootd-config
      COMMAND sed -e \"s/__VERSION__/${XROOTD_VERSION}/\"
      COMMAND sed -e \"s|__INCLUDEDIR__|${CMAKE_INSTALL_INCLUDEDIR}|\"
      COMMAND sed -e \"s/__PLUGIN_VERSION__/${PLUGIN_VERSION}/\"
      COMMAND sed -e \"s|__PREFIX__|${CMAKE_INSTALL_PREFIX}|\"
      OUTPUT_FILE \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/xrootd-config )
    EXECUTE_PROCESS(
      COMMAND chmod 755 \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/xrootd-config )"
)

#-------------------------------------------------------------------------------
# Post process man pages
#-------------------------------------------------------------------------------
install(
  CODE "
    FILE(GLOB MANPAGES
      \"\$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man[1,8]/*.[1,8]\" )
    FOREACH(MANPAGE \${MANPAGES})
      MESSAGE( \"-- Processing: \" \${MANPAGE} )
      EXECUTE_PROCESS(
        COMMAND cat \${MANPAGE}
        COMMAND sed  -e \"s/__VERSION__/${XROOTD_VERSION}/\"
        OUTPUT_FILE \${MANPAGE}.new )
      EXECUTE_PROCESS(
        COMMAND mv -f \${MANPAGE}.new \${MANPAGE} )
    ENDFOREACH()"
)
endif()
