# XrdClS3Obj only exists when libcurl was found
if(NOT TARGET XrdClS3Obj)
  return()
endif()

add_unit_tests(
  SOURCES
    XrdClS3DownloadHandler.cc
    XrdClS3Factory.cc
    XrdClS3File.cc
  LIBRARIES
    XrdClS3Obj
)
