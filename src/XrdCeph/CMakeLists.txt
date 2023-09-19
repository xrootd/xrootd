#-------------------------------------------------------------------------------
# Project description
#-------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16...3.25)

project( xrootd-ceph )

set( CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/src
 ${PROJECT_SOURCE_DIR}/cmake )

if( NOT XRDCEPH_SUBMODULE )
  if(NOT (CMAKE_VERSION VERSION_LESS "3.1"))
    cmake_policy(SET CMP0054 OLD)
  endif()
endif()

include( XRootDUtils )
CheckBuildDirectory()

include( XRootDOSDefs )
include( XRootDDefaults )
include( XRootDFindLibs )

add_definitions( -DXRDPLUGIN_SOVERSION="${PLUGIN_VERSION}" )

#-------------------------------------------------------------------------------
# Generate the version header
#-------------------------------------------------------------------------------
if( NOT XRDCEPH_SUBMODULE )
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
endif()

#-------------------------------------------------------------------------------
# Build in subdirectories
#-------------------------------------------------------------------------------
add_subdirectory( src )

if( BUILD_TESTS )
  ENABLE_TESTING()
  add_subdirectory( tests )
endif()

include( XRootDSummary )
