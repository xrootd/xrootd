set(XrdSecjwt XrdSecjwt-${PLUGIN_VERSION})

# Client-only builds (the Python wheels) do not ship libcurl. Skip the plugin
# then; a full build with FORCE_ENABLED still fails if CURL is missing.
include(${CMAKE_SOURCE_DIR}/cmake/XrdCurlLink.cmake)

# Prefer curl's CMake package. It records the libraries a static libcurl
# needs. CMake 3.18's FindCURL module does not.
if(FORCE_ENABLED)
  find_package(CURL CONFIG QUIET)
  if(NOT CURL_FOUND)
    find_package(CURL MODULE REQUIRED)
  endif()
else()
  find_package(CURL CONFIG QUIET)
  if(NOT CURL_FOUND)
    find_package(CURL MODULE)
  endif()
endif()

if(NOT CURL_FOUND)
  return()
endif()

find_package(OpenSSL REQUIRED)

add_library(XrdSecjwtCore SHARED
  XrdSecJWT.cc)

set_target_properties(XrdSecjwtCore PROPERTIES
  OUTPUT_NAME "XrdSecjwtCore-${PLUGIN_VERSION}")

target_link_libraries(XrdSecjwtCore
  PRIVATE XrdUtils OpenSSL::Crypto)
xrd_target_link_curl(XrdSecjwtCore)

add_library(${XrdSecjwt} MODULE
  XrdSecProtocoljwt.cc)

target_link_libraries(${XrdSecjwt}
  PRIVATE XrdSecjwtCore XrdUtils OpenSSL::Crypto)
xrd_target_link_curl(${XrdSecjwt})

install(TARGETS XrdSecjwtCore ${XrdSecjwt} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
