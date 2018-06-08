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
    XrdHttp-${PLUGIN_VERSION}
    dl
    pthread
    ${CURL_LIBRARIES} )

  set_target_properties(
    ${LIB_XRD_TPC}
    PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES ""
    LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/XrdTpc/export-lib-symbols"
    COMPILE_DEFINITIONS "XRD_CHUNK_RESP")

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_TPC}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

endif()
