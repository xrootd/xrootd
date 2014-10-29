
include( XRootDCommon )
include_directories( ${CPPUNIT_INCLUDE_DIRS} )

set( LIB_XRD_CL_TEST_MONITOR XrdClTestMonitor-${PLUGIN_VERSION} )

add_library(
  XrdClTestsHelper SHARED
  Server.cc              Server.hh
  Utils.cc               Utils.hh
  TestEnv.cc             TestEnv.hh
  CppUnitXrdHelpers.hh
)

target_link_libraries(
  XrdClTestsHelper
  pthread
  ${CPPUNIT_LIBRARIES}
  ${ZLIB_LIBRARY}
  XrdCl )


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
)

target_link_libraries(
  XrdClTests
  XrdClTestsHelper
  pthread
  ${CPPUNIT_LIBRARIES}
  ${ZLIB_LIBRARY}
  XrdCl )

add_library(
  ${LIB_XRD_CL_TEST_MONITOR} MODULE
  MonitorTestLib.cc
)

target_link_libraries(
  ${LIB_XRD_CL_TEST_MONITOR}
  XrdClTestsHelper
  XrdCl )

add_executable( text-runner TextRunner.cc PathProcessor.hh )
target_link_libraries( text-runner dl ${CPPUNIT_LIBRARIES} pthread )

add_custom_target(
  check
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/printenv.sh
  COMMAND ${CMAKE_CTEST_COMMAND} )

ADD_TEST( URLTest                   ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/UtilsTest/UtilsTest::URLTest")
ADD_TEST( AnyTest                   ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/UtilsTest/UtilsTest::AnyTest")
ADD_TEST( TaskManagerTest           ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/UtilsTest/UtilsTest::TaskManagerTest")
ADD_TEST( SIDManagerTest            ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/UtilsTest/UtilsTest::SIDManagerTest")
ADD_TEST( TransferTest              ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/SocketTest/SocketTest::TransferTest")
ADD_TEST( FunctionTestBuiltIn       ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/PollerTest/PollerTest::FunctionTestBuiltIn")

if( LIBEVENT_FOUND )
ADD_TEST( FunctionTestLibEvent       ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/PollerTest/PollerTest::FunctionTestLibEvent")
endif()

ADD_TEST( PostMasterTest            ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/PostMasterTest/PostMasterTest::FunctionalTest")
ADD_TEST( PingIPv6Test              ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/PostMasterTest/PostMasterTest::PingIPv6")
ADD_TEST( ThreadingTest             ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/PostMasterTest/PostMasterTest::ThreadingTest")
ADD_TEST( MultiIPConnectTest        ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/PostMasterTest/PostMasterTest::MultiIPConnectionTest")
ADD_TEST( LocateTest                ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::LocateTest")
ADD_TEST( MvTest                    ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::MvTest")
ADD_TEST( ServerQueryTest           ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::ServerQueryTest")
ADD_TEST( TruncateRmTest            ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::TruncateRmTest")
ADD_TEST( MkdirRmdirTest            ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::MkdirRmdirTest")
ADD_TEST( ChmodTest                 ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::ChmodTest")
ADD_TEST( PingTestTest              ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::PingTest")
ADD_TEST( StatTest                  ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::StatTest")
ADD_TEST( StatVFSTest               ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::StatVFSTest")
ADD_TEST( ProtocolTest              ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::ProtocolTest")
ADD_TEST( DeepLocateTest            ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::DeepLocateTest")
ADD_TEST( DirListTest               ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileSystemTest/FileSystemTest::DirListTest")
ADD_TEST( RedirectReturnTest        ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileTest/FileTest::RedirectReturnTest")
ADD_TEST( ReadTest                  ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileTest/FileTest::ReadTest")
ADD_TEST( WriteTest                 ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileTest/FileTest::WriteTest")
ADD_TEST( VectorReadTest            ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileTest/FileTest::VectorReadTest")
ADD_TEST( DownloadTest              ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileCopyTest/FileCopyTest::DownloadTest")
ADD_TEST( MultiStrDownloadTest      ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileCopyTest/FileCopyTest::MultiStreamDownloadTest")
ADD_TEST( UploadTest                ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/FileCopyTest/FileCopyTest::UploadTest")
ADD_TEST( ThreadingReadTest         ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/ThreadingTest/ThreadingTest::ReadTest")
ADD_TEST( MultiStrThreadingReadTest ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/ThreadingTest/ThreadingTest::MultiStreamReadTest")
ADD_TEST( ThreadingReadForkTest     ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/ThreadingTest/ThreadingTest::ReadForkTest")
ADD_TEST( MultiStrThreadingReadForkTest ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/ThreadingTest/ThreadingTest::MultiStreamReadForkTest")
ADD_TEST( MultiStrThreadingReadMonitorTest ${CMAKE_CURRENT_BINARY_DIR}/text-runner ./libXrdClTests.so "All Tests/ThreadingTest/ThreadingTest::MultiStreamReadMonitorTest")

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdClTests XrdClTestsHelper ${LIB_XRD_CL_TEST_MONITOR} text-runner
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
