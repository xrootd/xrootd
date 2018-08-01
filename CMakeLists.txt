#-------------------------------------------------------------------------------
# Project description
#-------------------------------------------------------------------------------
cmake_minimum_required( VERSION 2.6 )

IF(CMAKE_VERSION VERSION_GREATER "2.8.12")
   CMAKE_POLICY(SET CMP0022 OLD)
ENDIF()

project( XRootD )

set( CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/src
 ${PROJECT_SOURCE_DIR}/cmake )

if(NOT (CMAKE_VERSION VERSION_LESS "3.1"))
  cmake_policy(SET CMP0054 OLD)
endif()

if(CMAKE_VERSION VERSION_LESS "3.0")
  SET(CMAKE_DL_LIBS "-ldl")
endif()

include( XRootDUtils )
CheckBuildDirectory()

include( XRootDOSDefs )
include( XRootDDefaults )
include( XRootDSystemCheck )
include( XRootDFindLibs )

add_definitions( -DXRDPLUGIN_SOVERSION="${PLUGIN_VERSION}" )

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
add_subdirectory( bindings )

if( BUILD_TESTS )
  ENABLE_TESTING()
  add_subdirectory( tests )
endif()

include( XRootDSummary )





#-------------------------------------------------------------------------------
# Configure an 'uninstall' target
#-------------------------------------------------------------------------------
CONFIGURE_FILE(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
