
#-------------------------------------------------------------------------------
# Include the subcomponents
#-------------------------------------------------------------------------------
include( XrdUtils )
include( XrdCrypto )

include( XrdSec )

if( BUILD_CRYPTO )
  include( XrdSecgsi )
endif()

if( BUILD_KRB5 )
  include( XrdSeckrb5 )
endif()

include( XrdClient )

if( ENABLE_XRDCL )
  add_subdirectory( XrdCl )
endif()

include( XrdOfs )
include( XrdServer )
include( XrdDaemons )
include( XrdFrm )

include( XrdPosix )
include( XrdFfs )
include( XrdPlugins )
include( XrdApps )
include( XrdCns )
include( XrdHeaders )

include( XrdFileCache )

if( BUILD_HTTP )
  include( XrdHttp )
endif()

#-------------------------------------------------------------------------------
# Install the utility scripts
#-------------------------------------------------------------------------------
install(
  FILES
  ${CMAKE_SOURCE_DIR}/utils/XrdCmsNotify.pm
  ${CMAKE_SOURCE_DIR}/utils/netchk
  ${CMAKE_SOURCE_DIR}/utils/XrdOlbMonPerf
  DESTINATION ${CMAKE_INSTALL_DATADIR}/xrootd/utils
  PERMISSIONS
  OWNER_EXECUTE OWNER_WRITE OWNER_READ
  GROUP_EXECUTE GROUP_READ
  WORLD_EXECUTE WORLD_READ )

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
