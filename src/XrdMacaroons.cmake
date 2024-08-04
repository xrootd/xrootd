
#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_MACAROONS  XrdMacaroons-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

if( BUILD_MACAROONS )
  add_library(
    ${LIB_XRD_MACAROONS}
    MODULE
    XrdMacaroons/XrdMacaroons.cc
    XrdMacaroons/XrdMacaroonsHandler.cc     XrdMacaroons/XrdMacaroonsHandler.hh
    XrdMacaroons/XrdMacaroonsAuthz.cc       XrdMacaroons/XrdMacaroonsAuthz.hh
    XrdMacaroons/XrdMacaroonsConfigure.cc)

  target_link_libraries(
    ${LIB_XRD_MACAROONS}
    PRIVATE
    XrdHttpUtils
    XrdUtils
    XrdServer
    uuid::uuid
    ${MACAROONS_LIB}
    ${JSON_LIBRARIES}
    ${XROOTD_HTTP_LIB}
    OpenSSL::Crypto
    ${CMAKE_DL_LIBS})

  target_include_directories(${LIB_XRD_MACAROONS}
    PRIVATE ${MACAROONS_INCLUDES} ${JSON_INCLUDE_DIRS})

  if( MacOSX )
    SET( MACAROONS_LINK_FLAGS "-Wl")
  else()
    SET( MACAROONS_LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/XrdMacaroons/export-lib-symbols" )
  endif()

  set_target_properties(
    ${LIB_XRD_MACAROONS}
    PROPERTIES
    LINK_FLAGS "${MACAROONS_LINK_FLAGS}")

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_MACAROONS}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

endif()
