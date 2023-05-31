add_library(
  XrdCephTests MODULE
  CephParsingTest.cc
)

target_link_libraries(
  XrdCephTests
  ${CMAKE_THREAD_LIBS_INIT}
  ${CPPUNIT_LIBRARIES}
  ZLIB::ZLIB
  XrdCephPosix )

target_include_directories( XrdCephTests PRIVATE ${CPPUNIT_INCLUDE_DIRS} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCephTests
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
