
#-------------------------------------------------------------------------------
# Include the subcomponents
#-------------------------------------------------------------------------------
if( XRDCL_LIB_ONLY )
  set ( XRDCL_ONLY TRUE )
endif()

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

if( ENABLE_XRDCL )
  add_subdirectory( XrdCl )
endif()

if( XRDCLHTTP_SUBMODULE )
  add_subdirectory( XrdClHttp )
endif()

if( NOT XRDCL_ONLY )
  include( XrdServer )
  include( XrdDaemons )
  include( XrdFrm )
  include( XrdPosix )
  include( XrdFfs )
  include( XrdPlugins )
  include( XrdSsi )
#  include( XrdCns ) TODO: this is using the old client, needs refactoring or needs to be removed!

  include( XrdPfc )

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
