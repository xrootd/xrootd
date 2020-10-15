#------------------------------------------------------------------------------
# Setup variables for the scitokens submodules
#------------------------------------------------------------------------------
cmake_minimum_required( VERSION 3.1 )

set( XROOTD_PLUGIN_VERSION ${PLUGIN_VERSION} )
set( XROOTD_INCLUDES   ${CMAKE_SOURCE_DIR}/src )
set( XROOTD_UTILS_LIB  ${CMAKE_BINARY_DIR}/src/libXrdUtils.so )
set( XROOTD_SERVER_LIB ${CMAKE_BINARY_DIR}/src/libXrdServer.so )
add_subdirectory( XrdSciTokens )
add_dependencies( XrdAccSciTokens XrdUtils )
add_dependencies( XrdAccSciTokens XrdServer )
