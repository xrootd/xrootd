#-------------------------------------------------------------------------------
# Define the OS variables
#-------------------------------------------------------------------------------

include( CheckCXXSourceRuns )

set( LINUX    FALSE )
set( KFREEBSD FALSE )
set( Hurd     FALSE )
set( MacOSX   FALSE )
set( Solaris  FALSE )

set( XrdClPipelines FALSE )

add_definitions( -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 )
set( LIBRARY_PATH_PREFIX "lib" )

#-------------------------------------------------------------------------------
# Enable c++14
#-------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 14)


#-------------------------------------------------------------------------------
# Enable XrdCl::Pipelines for clang compiler
# Note: once we move to c++14 globaly we can remove this
#-------------------------------------------------------------------------------
if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
  set( XrdClPipelines TRUE )
endif()

#-------------------------------------------------------------------------------
# GCC
#-------------------------------------------------------------------------------
if( CMAKE_COMPILER_IS_GNUCXX )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra" )
  #-----------------------------------------------------------------------------
  # Set -Werror only for Debug (or undefined) build type or if we have been
  # explicitly asked to do so
  #-----------------------------------------------------------------------------
  if( ( CMAKE_BUILD_TYPE STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL ""
        OR FORCE_WERROR ) )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror" )
  endif()
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter" )
  # gcc 4.1 is retarded
  execute_process( COMMAND ${CMAKE_C_COMPILER} -dumpversion
                   OUTPUT_VARIABLE GCC_VERSION )
  if( (GCC_VERSION VERSION_GREATER 4.1 OR GCC_VERSION VERSION_EQUAL 4.1)
      AND GCC_VERSION VERSION_LESS 4.2 )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing" )
  endif()

  if( GCC_VERSION VERSION_GREATER 4.8.0 )
  	set( XrdClPipelines TRUE )
  endif()
endif()

#-------------------------------------------------------------------------------
# Linux
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
  set( LINUX TRUE )
  include( GNUInstallDirs )
  set( EXTRA_LIBS rt )
endif()

#-------------------------------------------------------------------------------
# GNU/kFreeBSD
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" )
  set( KFREEBSD TRUE )
  include( GNUInstallDirs )
  set( EXTRA_LIBS rt )
endif()

#-------------------------------------------------------------------------------
# GNU/Hurd
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "GNU" )
  set( Hurd TRUE )
  include( GNUInstallDirs )
  set( EXTRA_LIBS rt )
endif()

#-------------------------------------------------------------------------------
# MacOSX
#-------------------------------------------------------------------------------
if( APPLE )
  set( MacOSX TRUE )
  set( XrdClPipelines TRUE )
  
  if( NOT DEFINED CMAKE_MACOSX_RPATH )
    set( CMAKE_MACOSX_RPATH 1 )
  endif()

  # this is here because of Apple deprecating openssl and krb5
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" )

  add_definitions( -DLT_MODULE_EXT=".dylib" )
  set( CMAKE_INSTALL_LIBDIR "lib" )
  set( CMAKE_INSTALL_BINDIR "bin" )
  set( CMAKE_INSTALL_MANDIR "share/man" )
  set( CMAKE_INSTALL_INCLUDEDIR "include" )
  set( CMAKE_INSTALL_DATADIR "share" )
endif()

#-------------------------------------------------------------------------------
# FreeBSD
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" )
  set( CMAKE_INSTALL_LIBDIR "lib" )
  set( CMAKE_INSTALL_BINDIR "bin" )
  set( CMAKE_INSTALL_MANDIR "man" )
  set( CMAKE_INSTALL_INCLUDEDIR "include" )
  set( CMAKE_INSTALL_DATADIR "share" )
endif()

#-------------------------------------------------------------------------------
# Solaris
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "SunOS" )
  define_default( FORCE_32BITS FALSE )
  set( CMAKE_INSTALL_LIBDIR "lib" )
  set( CMAKE_INSTALL_BINDIR "bin" )
  set( CMAKE_INSTALL_MANDIR "man" )
  set( CMAKE_INSTALL_INCLUDEDIR "include" )
  set( CMAKE_INSTALL_DATADIR "share" )
  set( Solaris TRUE )
  add_definitions( -D__solaris__=1 )
  add_definitions( -DSUNCC -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS )
  set( EXTRA_LIBS rt  Crun Cstd )

  set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fast" )
  set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fast" )

  define_solaris_flavor()

  #-----------------------------------------------------------------------------
  # Define solaris version
  #-----------------------------------------------------------------------------
  execute_process( COMMAND uname -r
                   OUTPUT_VARIABLE SOLARIS_VER )
  string( REPLACE "." ";" SOLARIS_VER_LIST ${SOLARIS_VER} )
  list( GET SOLARIS_VER_LIST 1 SOLARIS_VERSION )
  string( REPLACE "\n" "" SOLARIS_VERSION ${SOLARIS_VERSION} )
  add_definitions( -DSOLARIS_VERSION=${SOLARIS_VERSION} )

  #-----------------------------------------------------------------------------
  # AMD64 (opteron)
  #-----------------------------------------------------------------------------
  if( ${SOLARIS_VERSION} STREQUAL "10" AND SOLARIS_AMD64 AND NOT FORCE_32BITS )
    set( CMAKE_CXX_FLAGS " -m64 -xtarget=opteron -xs ${CMAKE_CXX_FLAGS} " )
    set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -G" )
    set( CMAKE_LIBRARY_PATH "/lib/64;/usr/lib/64" )
    add_definitions( -DSUNX86 )
    set( LIB_SEARCH_OPTIONS NO_DEFAULT_PATH )
    set( LIBRARY_PATH_PREFIX "lib/64" )
  endif()

  #-----------------------------------------------------------------------------
  # Check if the SunCC compiler can do optimizations
  #-----------------------------------------------------------------------------
  check_cxx_source_runs(
  "
    int main()
    {
      #if __SUNPRO_CC > 0x5100
      return 0;
      #else
      return 1;
      #endif
    }
  "
  SUNCC_CAN_DO_OPTS )

endif()
