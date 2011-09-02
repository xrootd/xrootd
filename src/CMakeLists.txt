
#-------------------------------------------------------------------------------
# Include the subcomponents
#-------------------------------------------------------------------------------
include( XrdUtils )
include( XrdCrypto )

include( XrdSec )

if( OPENSSL_FOUND )
include( XrdSecgsi )
endif()

if( KERBEROS5_FOUND )
include( XrdSeckrb5 )
endif()

include( XrdClient )

include( XrdOfs )
include( XrdServer )
include( XrdDaemons )
include( XrdFrm )

include( XrdPosix )
include( XrdFfs )
include( XrdPlugins )
include( XrdApps )
include( XrdCns )

#-------------------------------------------------------------------------------
# Install the version and the protocol headers
#-------------------------------------------------------------------------------
install(
  DIRECTORY      XProtocol/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XProtocol
  FILES_MATCHING
  PATTERN "*.hh" )

install(
  FILES
  ${CMAKE_BINARY_DIR}/src/XrdVersion.hh
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xrootd
  PERMISSIONS OWNER_READ OWNER_WRITE )
