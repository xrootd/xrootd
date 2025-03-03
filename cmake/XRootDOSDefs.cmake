#-------------------------------------------------------------------------------
# Define the OS variables
#-------------------------------------------------------------------------------

set( LINUX    FALSE )
set( KFREEBSD FALSE )
set( Hurd     FALSE )
set( Solaris  FALSE )

add_definitions( -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 )
define_default( LIBRARY_PATH_PREFIX "lib" )

#-------------------------------------------------------------------------------
# Enable c++14
#-------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ Standard")
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if( ENABLE_ASAN )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fsanitize=address")
endif()

if( ENABLE_TSAN )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fsanitize=thread")
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
endif()

# Disable warnings with nvc++ (for when we are built as ROOT built-in dependency)

if(CMAKE_CXX_COMPILER_ID MATCHES "NVHPC")
  add_compile_options(-w)
endif()

#-------------------------------------------------------------------------------
# Linux
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
  set( LINUX TRUE )
  include( GNUInstallDirs )
  set( EXTRA_LIBS rt )

  # Check for musl libc with the compiler, since it provides way to detect it
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpmachine
    OUTPUT_VARIABLE TARGET_TRIPLE ERROR_VARIABLE TARGET_ERROR)

  if (NOT TARGET_ERROR)
    if ("${TARGET_TRIPLE}" MATCHES "musl")
      message(STATUS "Detected musl libc")
      add_definitions(-DMUSL=1)
    endif()
  else()
    message(WARNING "Could not detect system information!")
  endif()

  unset(TARGET_ERROR)
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
# macOS
#-------------------------------------------------------------------------------
if( APPLE )
  set(CMAKE_MACOSX_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  set(CMAKE_INSTALL_RPATH "@loader_path/../lib" CACHE STRING "Install RPATH")

  # this is here because of Apple deprecating openssl and krb5
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" )

  add_definitions( -DLT_MODULE_EXT=".dylib" )
  define_default( CMAKE_INSTALL_LIBDIR "lib" )
  define_default( CMAKE_INSTALL_BINDIR "bin" )
  define_default( CMAKE_INSTALL_MANDIR "share/man" )
  define_default( CMAKE_INSTALL_INCLUDEDIR "include" )
  define_default( CMAKE_INSTALL_DATADIR "share" )
endif()

#-------------------------------------------------------------------------------
# FreeBSD
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" )
  define_default( CMAKE_INSTALL_LIBDIR "lib" )
  define_default( CMAKE_INSTALL_BINDIR "bin" )
  define_default( CMAKE_INSTALL_MANDIR "man" )
  define_default( CMAKE_INSTALL_INCLUDEDIR "include" )
  define_default( CMAKE_INSTALL_DATADIR "share" )
endif()

#-------------------------------------------------------------------------------
# Solaris
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "SunOS" )
  define_default( FORCE_32BITS FALSE )
  define_default( CMAKE_INSTALL_LIBDIR "lib" )
  define_default( CMAKE_INSTALL_BINDIR "bin" )
  define_default( CMAKE_INSTALL_MANDIR "man" )
  define_default( CMAKE_INSTALL_INCLUDEDIR "include" )
  define_default( CMAKE_INSTALL_DATADIR "share" )
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
    define_default( CMAKE_LIBRARY_PATH "/lib/64;/usr/lib/64" )
    add_definitions( -DSUNX86 )
    set( LIB_SEARCH_OPTIONS NO_DEFAULT_PATH )
    define_default( LIBRARY_PATH_PREFIX "lib/64" )
  endif()
endif()
