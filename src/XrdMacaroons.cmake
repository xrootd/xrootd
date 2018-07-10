include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_MACAROONS  XrdMacaroons-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

if( BUILD_MACAROONS )
  include_directories(${MACAROONS_INCLUDES} ${JSON_INCLUDE_DIRS} ${UUID_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

  add_library(
    ${LIB_XRD_MACAROONS}
    MODULE
    XrdMacaroons/macaroons.cpp
    XrdMacaroons/handler.cpp     XrdMacaroons/handler.hh
    XrdMacaroons/authz.cpp       XrdMacaroons/authz.hh
    XrdMacaroons//configure.cpp)

  target_link_libraries(
    ${LIB_XRD_MACAROONS} -ldl
    XrdUtils
    XrdServer
    ${MACAROONS_LIB}
    ${JSON_LIBRARIES}
    ${XROOTD_HTTP_LIB}
    ${UUID_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARY})

  if( MacOSX )
    SET( MACAROONS_LINK_FLAGS "-Wl")
  else()
    SET( MACAROONS_LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/XrdMacaroons/export-lib-symbols" )
  endif()

  set_target_properties(
    ${LIB_XRD_MACAROONS}
    PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES ""
    LINK_FLAGS "${MACAROONS_LINK_FLAGS}")

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_MACAROONS}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

endif()
