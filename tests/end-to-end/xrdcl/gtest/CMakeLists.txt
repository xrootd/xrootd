# Tests of the XrdCl API against the cluster of xrdcl/cluster.bats, which
# starts the cluster, makes the test data, and runs each test of this program
# on its own.
add_executable(xrdcl-cluster-tests
  IdentityPlugIn.cc
  XrdClFileClone.cc
  XrdClFileCopyTest.cc
  XrdClFileSystemTest.cc
  XrdClFileTest.cc
  XrdClOperationsWorkflowTest.cc
  XrdClPostMasterTest.cc
  XrdClTTLManyStreams.cc
  XrdClThreadingTest.cc
  XrdClZip.cc
)

target_link_libraries(xrdcl-cluster-tests
  XrdTestUtils GTest::gtest GTest::gtest_main)
