cmake_minimum_required (VERSION 2.6)



# The version number
set (XRDHTTP_VERSION_MAJOR 1)
set (XRDHTTP_VERSION_MINOR 0)
set (XRDHTTP_VERSION_PATCH 0)




# include the directory itself as a path to include directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Configure a header file to pass some of the CMake settings to the source code
# This is useful in order to handle the various system-related defines.
configure_file (
               "${PROJECT_SOURCE_DIR}/src/XrdHTTP/XrdHttpConfig.h.in"
               "${PROJECT_BINARY_DIR}/src/XrdHTTP/XrdHttpConfig.h"
)



#
# VOMS libraries
#
FIND_LIBRARY(VOMS_API_LIB vomsapi)
message(STATUS "Found Voms2 API : ${VOMS_API_LIB}")
if (NOT VOMS_API_LIB)
  message(STATUS "Voms API lib not found. The XrdHTTPVOMS plugin will not be compiled.")
ENDIF()


# -------------------------
# Local targets
# -------------------------

set(XrdHttp_SOURCES XrdHttpProtocol.cc XrdHttpReq.cc XrdHttpTrace.cc XrdHttpUtils.cc)
set(XrdHttpVOMS_SOURCES XrdHttpVoms.cc XrdHttpTrace.cc)

# Add the binary tree to the search path for include files
# so that we can find our Config header file
include_directories(${PROJECT_BINARY_DIR})


#
# Our target is a library to be loaded as a plugin by Xrootd
#
add_library(XrdHttp SHARED ${XrdHttp_SOURCES})
set_target_properties(XrdHttp PROPERTIES
 VERSION "${XRDHTTP_VERSION_MAJOR}.${XRDHTTP_VERSION_MINOR}.${XRDHTTP_VERSION_PATCH}"
 SOVERSION "${XRDHTTP_VERSION_MAJOR}.${XRDHTTP_VERSION_MINOR}")

target_link_libraries(XrdHttp XrdMain dl ssl )


if (VOMS_API_LIB)
  #
  # Our target is also a library to be loaded as a plugin by XrdHTTP,
  # whose goal is to extract credentials from the connection
  #
  add_library(XrdHttpVOMS SHARED ${XrdHttpVOMS_SOURCES})
  set_target_properties(XrdHttpVOMS PROPERTIES
   VERSION "${XRDHTTP_VERSION_MAJOR}.${XRDHTTP_VERSION_MINOR}.${XRDHTTP_VERSION_PATCH}"
   SOVERSION "${XRDHTTP_VERSION_MAJOR}.${XRDHTTP_VERSION_MINOR}")

  target_link_libraries(XrdHttpVOMS ${VOMS_API_LIB} XrdMain ssl )

  # Install directive. This is the VOMS stuff
  install(TARGETS XrdHttpVOMS
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT core)

endif()




# Install directive. This is the library core
install(TARGETS XrdHttp
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  COMPONENT core)

