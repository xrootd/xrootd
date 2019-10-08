#-------------------------------------------------------------------------------
# Define the OS variables
#-------------------------------------------------------------------------------

include( CheckCXXSourceRuns )

add_definitions( -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 )
set( LIBRARY_PATH_PREFIX "lib" )

#-------------------------------------------------------------------------------
# GCC
#-------------------------------------------------------------------------------
if( CMAKE_COMPILER_IS_GNUCXX )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter" )
  # gcc 4.1 is retarded
  execute_process( COMMAND ${CMAKE_C_COMPILER} -dumpversion
                   OUTPUT_VARIABLE GCC_VERSION )
  if( (GCC_VERSION VERSION_GREATER 4.1 OR GCC_VERSION VERSION_EQUAL 4.1)
      AND GCC_VERSION VERSION_LESS 4.2 )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing" )
  endif()

  # for 4.9.3 or greater the 'omit-frame-pointer' 
  # interfears  with custom semaphore implementation
  if( (GCC_VERSION VERSION_GREATER 4.9.2) AND (USE_LIBC_SEMAPHORE EQUAL 0) )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer" )
  endif()

  # gcc 6.0 is more pedantic
  if( GCC_VERSION VERSION_GREATER 6.0 OR GCC_VERSION VERSION_EQUAL 6.0 )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=misleading-indentation" )
  endif()
endif()

#-------------------------------------------------------------------------------
# Linux
#-------------------------------------------------------------------------------
set( Linux TRUE )
include( GNUInstallDirs )
add_definitions( -D__linux__=1 )
set( EXTRA_LIBS rt )

