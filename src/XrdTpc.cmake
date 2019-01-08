include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_TPC       XrdHttpTPC-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

if( BUILD_TPC )
  #-----------------------------------------------------------------------------
  # The XrdHttp library
  #-----------------------------------------------------------------------------
  include_directories( ${CURL_INCLUDE_DIRS} )

  # On newer versions of libcurl, we can use pipelining of requests.
  include (CheckCSourceCompiles)
  SET( CMAKE_REQUIRED_INCLUDES "${CURL_INCLUDE_DIRS}" )
  check_c_source_compiles("#include <curl/multi.h>

  int main(int argc, char** argv)
  {
    (void)argv;
    int foo = CURLMOPT_MAX_HOST_CONNECTIONS;
    return 0;
  }
  " CURL_PIPELINING
  )
  set(XRD_COMPILE_DEFS "XRD_CHUNK_RESP")
  if ( CURL_PIPELINING )
    set(XRD_COMPILE_DEFS ${XRD_COMPILE_DEFS} "USE_PIPELINING")
  endif ()
  
  add_library(
    ${LIB_XRD_TPC}
    MODULE
    XrdTpc/XrdTpcConfigure.cc
    XrdTpc/XrdTpcMultistream.cc
    XrdTpc/XrdTpcCurlMulti.cc     XrdTpc/XrdTpcCurlMulti.hh
    XrdTpc/XrdTpcState.cc         XrdTpc/XrdTpcState.hh
    XrdTpc/XrdTpcStream.cc        XrdTpc/XrdTpcStream.hh
    XrdTpc/XrdTpcTPC.cc           XrdTpc/XrdTpcTPC.hh)

  target_link_libraries(
    ${LIB_XRD_TPC}
    XrdServer
    XrdUtils
    XrdHttpUtils
    ${CMAKE_DL_LIBS}
    pthread
    ${CURL_LIBRARIES} )

  if( MacOSX )
    set( TPC_LINK_FLAGS, "-Wl" )
  else()
    set( TPC_LINK_FLAGS, "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/XrdTpc/export-lib-symbols" )
  endif()

  set_target_properties(
    ${LIB_XRD_TPC}
    PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES ""
    LINK_FLAGS "${TPC_LINK_FLAGS}"
    COMPILE_DEFINITIONS "${XRD_COMPILE_DEFS}")

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_TPC}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

endif()
