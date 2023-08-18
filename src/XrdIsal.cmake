if(USE_SYSTEM_ISAL)
  find_package(isal REQUIRED)
endif()

if(ISAL_FOUND)
  return()
endif()

#-------------------------------------------------------------------------------
# Build isa-l
#-------------------------------------------------------------------------------

include(ExternalProject)
include(FindPackageHandleStandardArgs)

set(ISAL_VERSION v2.30.0)
message(STATUS "Building ISAL: ${ISAL_VERSION}")

set(ISAL_ROOT "${CMAKE_BINARY_DIR}/isa-l")
set(ISAL_LIBRARY "${ISAL_ROOT}/.libs/libisal.a")
set(ISAL_INCLUDE_DIRS "${ISAL_ROOT}")

ExternalProject_add(isa-l
  URL https://github.com/intel/isa-l/archive/refs/tags/${ISAL_VERSION}.tar.gz
  URL_HASH SHA256=bcf592c04fdfa19e723d2adf53d3e0f4efd5b956bb618fed54a1108d76a6eb56
  SOURCE_DIR        ${CMAKE_BINARY_DIR}/isa-l
  BUILD_IN_SOURCE   1
  CONFIGURE_COMMAND ./autogen.sh COMMAND ./configure --with-pic
  BUILD_COMMAND     make -j ${CMAKE_BUILD_PARALLEL_LEVEL}
  INSTALL_COMMAND   ${CMAKE_COMMAND} -E copy_directory ${ISAL_ROOT}/include ${ISAL_ROOT}/isa-l
  BUILD_BYPRODUCTS  ${ISAL_LIBRARY} ${ISAL_INCLUDE_DIRS}
  LOG_DOWNLOAD 1 LOG_CONFIGURE 1 LOG_BUILD 1 LOG_INSTALL 1
)

add_library(isal INTERFACE)
add_dependencies(isal isa-l)

target_link_libraries(isal INTERFACE $<BUILD_INTERFACE:${ISAL_LIBRARY}>)
target_include_directories(isal INTERFACE $<BUILD_INTERFACE:${ISAL_INCLUDE_DIRS}>)

set(ISAL_LIBRARIES isal CACHE INTERNAL "" FORCE)
set(ISAL_INCLUDE_DIRS ${ISAL_INCLUDE_DIRS} CACHE INTERNAL "" FORCE)

# Emulate what happens when find_package(isal) succeeds
find_package_handle_standard_args(isal
  REQUIRED_VARS ISAL_INCLUDE_DIRS ISAL_LIBRARIES VERSION_VAR ISAL_VERSION)
