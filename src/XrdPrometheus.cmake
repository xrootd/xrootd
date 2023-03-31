include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_PROMETHEUS  XrdPrometheus-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

if( BUILD_PROMETHEUS )
  include_directories(${PROMETHEUS_INCLUDE_DIRS})

  add_library(
    ${LIB_XRD_PROMETHEUS}
    MODULE
    XrdPrometheus/XrdPrometheus.cc
    XrdPrometheus/XrdPrometheusConfigure.cc)

  target_link_libraries(
    ${LIB_XRD_PROMETHEUS} ${CMAKE_DL_LIBS}
    XrdHttpUtils
    XrdUtils
    XrdServer
    XrdXml
    ${PROMETHEUS_LIBRARIES}
    ${XROOTD_HTTP_LIB})

  if( MacOSX )
    SET( PROMETHEUS_LINK_FLAGS "-Wl")
  else()
    SET( PROMETHEUS_LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/XrdPrometheus/export-lib-symbols" )
  endif()

  set_target_properties(
    ${LIB_XRD_PROMETHEUS}
    PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    LINK_INTERFACE_LIBRARIES ""
    LINK_FLAGS "${PROMETHEUS_LINK_FLAGS}")

  target_link_directories(
    ${LIB_XRD_PROMETHEUS}
    PRIVATE "${PROMETHEUS_LIBRARY_DIRS}")

  #-----------------------------------------------------------------------------
  # Install
  #-----------------------------------------------------------------------------
  install(
    TARGETS ${LIB_XRD_PROMETHEUS}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

endif()
