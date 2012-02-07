include_directories( ${CPPUNIT_INCLUDE_DIRS} )
include_directories( ../ ${XROOTD_INCLUDES}  )
link_directories( ${XROOTD_LIB_DIR} )

add_library(
  XrdClTests MODULE
  Server.cc          Server.hh
  Utils.cc           Utils.hh
  TestEnv.cc         TestEnv.hh
  UtilsTest.cc
  SocketTest.cc
  PollerTest.cc
  PostMasterTest.cc
  QueryTest.cc
  FileTest.cc
)

target_link_libraries(
  XrdClTests
  pthread
  ${CPPUNIT_LIBRARIES}
  ${ZLIB_LIBRARY}
  XrdCl )

add_executable( text-runner TextRunner.cc PathProcessor.hh )
target_link_libraries( text-runner dl ${CPPUNIT_LIBRARIES} pthread )
