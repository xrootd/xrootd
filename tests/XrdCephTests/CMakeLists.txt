include( XRootDCommon )
include_directories( ${CPPUNIT_INCLUDE_DIRS} )

add_library(
  XrdCephTests MODULE
  CephParsingTest.cc
)

target_link_libraries(
  XrdCephTests
  ${CMAKE_THREAD_LIBS_INIT}
  ${CPPUNIT_LIBRARIES}
  ${ZLIB_LIBRARIES}
  XrdCephPosix )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCephTests
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
