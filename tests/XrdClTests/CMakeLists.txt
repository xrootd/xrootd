
include( XRootDCommon )
include_directories( ${CPPUNIT_INCLUDE_DIRS} ../common)

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
  ${ZLIB_LIBRARIES}
  XrdCl )

add_library(
  ${LIB_XRD_CL_TEST_MONITOR} MODULE
  MonitorTestLib.cc
)

target_link_libraries(
  ${LIB_XRD_CL_TEST_MONITOR}
  XrdClTestsHelper
  XrdCl )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdClTests ${LIB_XRD_CL_TEST_MONITOR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
