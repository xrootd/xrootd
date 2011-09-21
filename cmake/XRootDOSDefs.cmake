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
