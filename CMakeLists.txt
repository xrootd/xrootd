#-------------------------------------------------------------------------------
# Project description
#-------------------------------------------------------------------------------
cmake_minimum_required( VERSION 2.6 )
project( XRootD )

set( CMAKE_BUILD_TYPE Debug )

set( CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/src
 ${PROJECT_SOURCE_DIR}/cmake )

#-------------------------------------------------------------------------------
# Define the OS variables
#-------------------------------------------------------------------------------
set( Linux    FALSE )
set( MacOSX   FALSE )
set( Solaris  FALSE )

add_definitions( -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 )

if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
  set( Linux TRUE )
  include( GNUInstallDirs )
  add_definitions( -D__LINUX__=1 )
  set( EXTRA_LIBS rt )
endif()

if( APPLE )
  set( MacOSX TRUE )
  add_definitions( -D__macos__=1 )
  add_definitions( -DLT_MODULE_EXT=".dylib" )
  set( CMAKE_INSTALL_LIBDIR "lib" )
  set( CMAKE_INSTALL_BINDIR "bin" )
  set( CMAKE_INSTALL_MANDIR "man" )
  set( CMAKE_INSTALL_INCLUDEDIR "include" )
  set( CMAKE_INSTALL_DATADIR "share" )
endif()

if( ${CMAKE_SYSTEM_NAME} STREQUAL "SunOS" )
  set( CMAKE_INSTALL_LIBDIR "lib" )
  set( CMAKE_INSTALL_BINDIR "bin" )
  set( CMAKE_INSTALL_MANDIR "man" )
  set( CMAKE_INSTALL_INCLUDEDIR "include" )
  set( CMAKE_INSTALL_DATADIR "share" )
  set( Solaris TRUE )
  add_definitions( -D__solaris__=1 )
  add_definitions( -DSUNCC -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS )
  set( EXTRA_LIBS rt  Crun Cstd )
endif()

#-------------------------------------------------------------------------------
# Check the system
#-------------------------------------------------------------------------------
include( XRootDSystemCheck )
xrootd_system_check()
find_package( Readline )
find_package( ZLIB )
find_package( OpenSSL )
find_package( Kerberos5 )
find_package( fuse )
find_package( PerlLibs )
find_package( SWIG )

if( READLINE_FOUND )
  add_definitions( -DHAVE_READLINE )
endif()

if( ZLIB_FOUND )
  add_definitions( -DHAVE_LIBZ )
endif()

if( OPENSSL_FOUND )
  add_definitions( -DHAVE_XRDCRYPTO )
endif()

if( FUSE_FOUND )
  add_definitions( -DHAVE_FUSE )
endif()

#-------------------------------------------------------------------------------
# Generate the version header
#-------------------------------------------------------------------------------
add_custom_target(
  XrdVersion.hh
  ALL
  ${CMAKE_SOURCE_DIR}/genversion.sh ${CMAKE_SOURCE_DIR} )

#-------------------------------------------------------------------------------
# Build in subdirectories
#-------------------------------------------------------------------------------
add_subdirectory( src )
