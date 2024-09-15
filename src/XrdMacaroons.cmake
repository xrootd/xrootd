
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
    XrdMacaroons/XrdMacaroonsGenerate.cc    XrdMacaroons/XrdMacaroonsGenerate.hh
    XrdMacaroons/XrdMacaroonsUtils.cc       XrdMacaroons/XrdMacaroonsUtils.hh
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
    # Note: as of CMake 3.26.1 on Mac OS X, the pkg_config module that detects json-c
    # will return only the library name in ${JSON_LIBRARIES}, not the full path.  Per
    # the CMake documentation (https://cmake.org/cmake/help/latest/command/target_link_directories.html),
    # this is incorrect.  Until it is fixed, we also set the link directory here.
    target_link_directories(${LIB_XRD_MACAROONS} PRIVATE "${JSON_LIBRARY_DIRS}")

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
