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
  XrdCeph )

add_executable( text-runner TextRunner.cc PathProcessor.hh )
target_link_libraries( text-runner dl ${CPPUNIT_LIBRARIES} pthread )

add_custom_target(
  check
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/printenv.sh
  COMMAND ${CMAKE_CTEST_COMMAND} )

ADD_TEST( ParamTest                  ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdCephTests.so "All Tests/CephParsingTest/CephParsingTest::ParamTest")
ADD_TEST( FileTest                   ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdCephTests.so "All Tests/CephParsingTest/CephParsingTest::FileTest")

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCephTests ${LIB_XRD_CEPH_TEST_MONITOR} text-runner
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
