# Tests of XrdClHttp against the HTTPS origin and cache of xrdclhttp/gtest.bats,
# which runs each test of these programs on its own. xrdcl-test uses only the
# interfaces of XrdCl; xrdcl-http-test links XrdClHttp.
add_executable(xrdcl-test
  ChecksumTest.cc
  DeleteTest.cc
  MkcolTest.cc
  MoveTest.cc
  ReadTest.cc
  WriteTest.cc
)

add_executable(xrdcl-http-test
  CopyTest.cc
  VectorReadTest.cc
)

target_link_libraries(xrdcl-test XrdClHttpTransferTest GTest::gtest_main)
target_link_libraries(xrdcl-http-test XrdClHttpObj XrdClHttpTransferTest GTest::gtest_main)
