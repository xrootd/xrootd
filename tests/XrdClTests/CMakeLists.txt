
add_subdirectory( tls )

set( LIB_XRD_CL_TEST_MONITOR XrdClTestMonitor-${PLUGIN_VERSION} )


if( XrdClPipelines )
  set( OperationsWorkflowTest OperationsWorkflowTest.cc )
endif()

add_library(
  XrdClTests MODULE
  UtilsTest.cc
  SocketTest.cc
  PollerTest.cc
  PostMasterTest.cc
  FileSystemTest.cc
  FileTest.cc
  FileCopyTest.cc
  ThreadingTest.cc
  IdentityPlugIn.cc
  LocalFileHandlerTest.cc
  
  ${OperationsWorkflowTest}
)

target_link_libraries(
  XrdClTests
  XrdClTestsHelper
  ${CMAKE_THREAD_LIBS_INIT}
  ${CPPUNIT_LIBRARIES}
  ZLIB::ZLIB
  XrdCl )

target_include_directories( XrdClTests PRIVATE ../common ${CPPUNIT_INCLUDE_DIRS} )

add_library(
  ${LIB_XRD_CL_TEST_MONITOR} MODULE
  MonitorTestLib.cc
)

target_link_libraries(
  ${LIB_XRD_CL_TEST_MONITOR}
  XrdClTestsHelper
  XrdCl )

target_include_directories( ${LIB_XRD_CL_TEST_MONITOR} PRIVATE ../common )

foreach(TEST_SUITE
    # File
    # FileCopy
    # FileSystem
    # LocalFileHandler
    Poller
    # PostMaster
    Socket
    # Threading
    Utils
    # Workflow
)
  add_test(NAME XrdCl::${TEST_SUITE}
    COMMAND $<TARGET_FILE:test-runner> $<TARGET_FILE:XrdClTests> "All Tests/${TEST_SUITE}Test")
  set_tests_properties(XrdCl::${TEST_SUITE} PROPERTIES RUN_SERIAL TRUE)
endforeach()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdClTests ${LIB_XRD_CL_TEST_MONITOR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
