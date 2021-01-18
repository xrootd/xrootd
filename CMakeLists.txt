#-------------------------------------------------------------------------------
# Project description
#-------------------------------------------------------------------------------
cmake_minimum_required( VERSION 3.1 )

project( XRootD )

set( CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/src
 ${PROJECT_SOURCE_DIR}/cmake )

if(NOT (CMAKE_VERSION VERSION_LESS "3.1"))
  cmake_policy(SET CMP0054 OLD)
endif()


#-------------------------------------------------------------------------------
# A 'plugins' phony target to simplify building build-tree binaries.
# Plugins are responsible for adding themselves to this target, where
# appropriate.
#-------------------------------------------------------------------------------
ADD_CUSTOM_TARGET(plugins)


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
if (USER_VERSION)
  set(XROOTD_VERSION "${USER_VERSION}")
else ()
execute_process(
  COMMAND ${CMAKE_SOURCE_DIR}/genversion.sh --print-only ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE XROOTD_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE )
endif()

add_custom_target(
  XrdVersion.hh
  ${CMAKE_SOURCE_DIR}/genversion.sh --version ${XROOTD_VERSION} ${CMAKE_SOURCE_DIR})

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
# Checkout the vomsxrd submodule
#-------------------------------------------------------------------------------
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  option(GIT_SUBMODULES "Check submodules during build" ON)
  if(GIT_SUBMODULES)
    message(STATUS "Submodule update")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
      message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
  endif()
endif()

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
# Install XRootDConfig.cmake module
#-------------------------------------------------------------------------------

configure_file( "cmake/XRootDConfig.cmake.in" "cmake/XRootDConfig.cmake" @ONLY )

install(
  FILES ${CMAKE_BINARY_DIR}/cmake/XRootDConfig.cmake
  DESTINATION ${CMAKE_INSTALL_DATADIR}/xrootd/cmake )

#-------------------------------------------------------------------------------
# Configure an 'uninstall' target
#-------------------------------------------------------------------------------
CONFIGURE_FILE(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
