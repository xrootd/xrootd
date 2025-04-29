macro( define_default variable value )
  if( NOT DEFINED ${variable} )
    set( ${variable} ${value} )
  endif()
endmacro()

# Set a default build type of RelWithDebInfo if not set
if(NOT GENERATOR_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
  if(NOT CMAKE_C_FLAGS AND NOT CMAKE_CXX_FLAGS)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "CMake build type for single-configuration generators" FORCE)
  endif()
endif()

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

# Set baseline warning level for GCC and Clang

add_compile_options(
  -Wall
  -Wextra
  $<$<NOT:$<BOOL:${APPLE}>>:-Wdeprecated>
  -Werror=null-dereference
  -Wno-unused-parameter
  -Wno-vla
)

# Disable some warnings currently triggered with Clang

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(
    -Wno-deprecated-copy-with-user-provided-dtor
    -Wno-unused-const-variable
    -Wno-unused-private-field
  )
endif()

# Disable warnings with nvc++ (for when we are built as ROOT built-in dependency)

if(CMAKE_CXX_COMPILER_ID MATCHES "NVHPC")
  add_compile_options(-w)
endif()

#-------------------------------------------------------------------------------
# Linux
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
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
  set( EXTRA_LIBS rt )
endif()

#-------------------------------------------------------------------------------
# GNU/Hurd
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "GNU" )
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
endif()

#-------------------------------------------------------------------------------
# Solaris
#-------------------------------------------------------------------------------
if( ${CMAKE_SYSTEM_NAME} STREQUAL "SunOS" )
  define_default( FORCE_32BITS FALSE )
  add_definitions( -D__solaris__=1 )
  add_definitions( -DSUNCC -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS )
  set( EXTRA_LIBS rt  Crun Cstd )

  set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fast" )
  set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fast" )

  execute_process( COMMAND isainfo
                   OUTPUT_VARIABLE SOLARIS_ARCH )
  string( REPLACE " " ";" SOLARIS_ARCH_LIST ${SOLARIS_ARCH} )

  # amd64 (opteron)
  list( FIND SOLARIS_ARCH_LIST amd64 SOLARIS_AMD64 )
  if( SOLARIS_AMD64 EQUAL -1 )
    set( SOLARIS_AMD64 FALSE )
  else()
    set( SOLARIS_AMD64 TRUE )
  endif()

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
