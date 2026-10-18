# XrdClS3Obj only exists when libcurl was found
if(NOT TARGET XrdClS3Obj)
  return()
endif()

add_executable(xrdcl-s3-unittest
  UrlParseTest.cc
  DownloadHandlerTest.cc
)

target_link_libraries(xrdcl-s3-unittest XrdClS3Obj GTest::gtest_main)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdcl-s3-unittest stdc++fs)
endif()

gtest_add_tests(TARGET xrdcl-s3-unittest TEST_LIST S3UnitTests)
