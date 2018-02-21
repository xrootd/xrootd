include_directories( ${CPPUNIT_INCLUDE_DIRS} )
include_directories( ${XROOTD_INCLUDE_DIR} )
include_directories( ${RADOS_INCLUDE_DIR} )
include_directories( ${CMAKE_SOURCE_DIR}/src )

message( "XROOTD_INCLUDE_DIR : ${XROOTD_INCLUDE_DIR}" )

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
