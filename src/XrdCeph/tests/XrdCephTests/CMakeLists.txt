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

target_include_directories(XrdCephTests PRIVATE
  ${CPPUNIT_INCLUDE_DIRS}
  ${RADOS_INCLUDE_DIR}
  ${XROOTD_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/src)

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCephTests
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
