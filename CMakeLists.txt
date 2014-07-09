#-------------------------------------------------------------------------------
# Project description
#-------------------------------------------------------------------------------
cmake_minimum_required( VERSION 2.6 )
project( XRootD )

set( CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/src
 ${PROJECT_SOURCE_DIR}/cmake )

include( XRootDUtils )
CheckBuildDirectory()

include( XRootDOSDefs )
include( XRootDDefaults )
include( XRootDSystemCheck )
include( XRootDFindLibs )

#-------------------------------------------------------------------------------
# Generate the version header
#-------------------------------------------------------------------------------
execute_process(
  COMMAND ${CMAKE_SOURCE_DIR}/genversion.sh --print-only ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XROOTD_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE )

add_custom_target(
  XrdVersion.hh
  ${CMAKE_SOURCE_DIR}/genversion.sh ${CMAKE_SOURCE_DIR} )

# sigh, yet another ugly hack :(
macro( add_library _target )
  _add_library( ${_target} ${ARGN} )
  add_dependencies( ${_target} XrdVersion.hh )
endmacro()

macro( add_executable _target )
  _add_executable( ${_target} ${ARGN} )
  add_dependencies( ${_target} XrdVersion.hh )
endmacro()

#-------------------------------------------------------------------------------
# Build in subdirectories
#-------------------------------------------------------------------------------
add_subdirectory( src )

if( BUILD_TESTS )
  ENABLE_TESTING()
  add_subdirectory( tests )
endif()

include( XRootDSummary )