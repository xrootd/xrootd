cmake_minimum_required(VERSION 3.16...3.31 FATAL_ERROR)

# Check for in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "XRootD cannot be built in-source! "
                      "Please run cmake <src-dir> outside the "
                      "source directory and be sure to remove "
                      "CMakeCache.txt or CMakeFiles if they "
                      "exist in the source directory.")
endif()

project( XRootD )

set( CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/src
 ${PROJECT_SOURCE_DIR}/cmake )

# Place build artifacts in ${CMAKE_BINARY_DIR}/{bin,lib}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE PATH "")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "")

include(XRootDVersion)

#-------------------------------------------------------------------------------
# A 'plugins' phony target to simplify building build-tree binaries.
# Plugins are responsible for adding themselves to this target, where
# appropriate.
#-------------------------------------------------------------------------------
ADD_CUSTOM_TARGET(plugins)

include(GNUInstallDirs)

include( XRootDOSDefs )
include( XRootDDefaults )
include( XRootDSystemCheck )
include( XRootDFindLibs )

#-------------------------------------------------------------------------------
# Generate the version header
#-------------------------------------------------------------------------------

configure_file(src/XrdVersion.hh.in src/XrdVersion.hh)

#-------------------------------------------------------------------------------
# Build in subdirectories
#-------------------------------------------------------------------------------

include_directories(BEFORE ${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/src)

include(CTest)

add_subdirectory(src)
add_subdirectory(bindings)
add_subdirectory(tests)

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
    CMAKE_INSTALL_BINDIR
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
