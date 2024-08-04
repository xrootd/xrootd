add_executable(xrdcl-unit-tests
  XrdClURL.cc
  XrdClPoller.cc
  XrdClSocket.cc
  XrdClUtilsTest.cc
  )

target_link_libraries(xrdcl-unit-tests
  XrdTestUtils GTest::GTest GTest::Main)

gtest_discover_tests(xrdcl-unit-tests TEST_PREFIX XrdCl::)

if(XRDCL_ONLY)
  return()
endif()

execute_process(COMMAND id -u OUTPUT_VARIABLE UID OUTPUT_STRIP_TRAILING_WHITESPACE)

if (UID EQUAL 0)
  return()
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tmp")

add_executable(xrdcl-cluster-tests
  IdentityPlugIn.cc
  # XrdClFileTest.cc
  # XrdClFileCopyTest.cc
  # XrdClFileSystemTest.cc
  # XrdClOperationsWorkflowTest.cc
  XrdClLocalFileHandlerTest.cc
  XrdClPostMasterTest.cc
  XrdClThreadingTest.cc
  XrdClZip.cc
  )

target_link_libraries(xrdcl-cluster-tests
  XrdTestUtils GTest::GTest GTest::Main)

gtest_discover_tests(xrdcl-cluster-tests TEST_PREFIX XrdCl::
  PROPERTIES DEPENDS XRootD_Cluster FIXTURES_REQUIRED XRootD_Cluster)

# tests to be run in serial, otherwise they are problematic
set(SERIAL_TESTS_FILES
  XrdClFileTest.cc
  XrdClFileCopyTest.cc
  XrdClFileSystemTest.cc
  XrdClOperationsWorkflowTest.cc
)

# create a separate executable target for each "problematic" test suite
foreach(i ${SERIAL_TESTS_FILES})
  add_executable(${i}-tests ${i} IdentityPlugIn.cc)

  target_link_libraries(${i}-tests
    XrdTestUtils GTest::GTest GTest::Main)

  gtest_discover_tests(${i}-tests TEST_PREFIX XrdCl::
    PROPERTIES DEPENDS XRootD_Cluster FIXTURES_REQUIRED XRootD_Cluster RUN_SERIAL 1)
endforeach()
