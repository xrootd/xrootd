#-------------------------------------------------------------------------------
# Project description
#-------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16...3.25)

project( XRootD )

set( CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/src
 ${PROJECT_SOURCE_DIR}/cmake )

include(XRootDVersion)

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

configure_file(src/XrdVersion.hh.in src/XrdVersion.hh)

#-------------------------------------------------------------------------------
# Build in subdirectories
#-------------------------------------------------------------------------------

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)

add_subdirectory( src )
add_subdirectory( bindings )

if( BUILD_TESTS )
  ENABLE_TESTING()
  add_subdirectory( tests )
endif()

add_subdirectory(docs)
add_subdirectory(utils)

include( XRootDSummary )


#-------------------------------------------------------------------------------
# Install XRootDConfig.cmake module
#-------------------------------------------------------------------------------

include(CMakePackageConfigHelpers)

write_basic_package_version_file(cmake/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${XRootD_VERSION} COMPATIBILITY SameMajorVersion)

configure_package_config_file(cmake/${PROJECT_NAME}Config.cmake.in cmake/${PROJECT_NAME}Config.cmake
  INSTALL_PREFIX
    ${CMAKE_INSTALL_PREFIX}
  INSTALL_DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  PATH_VARS
    CMAKE_INSTALL_INCLUDEDIR
    CMAKE_INSTALL_LIBDIR
    CMAKE_INSTALL_DATADIR
)

install(DIRECTORY ${PROJECT_BINARY_DIR}/cmake/
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

#-------------------------------------------------------------------------------
# Configure an 'uninstall' target
#-------------------------------------------------------------------------------
CONFIGURE_FILE(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
