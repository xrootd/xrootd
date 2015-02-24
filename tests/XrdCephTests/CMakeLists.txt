include( XRootDCommon )
include_directories( ${CPPUNIT_INCLUDE_DIRS} )

set( LIB_XRD_CL_TEST_MONITOR XrdClTestMonitor-${PLUGIN_VERSION} )

add_library(
  XrdCephTests MODULE
  CephParsingTest.cc
)

target_link_libraries(
  XrdCephTests
  pthread
  ${CPPUNIT_LIBRARIES}
  ${ZLIB_LIBRARY}
  XrdCephPosix )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCephTests
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
