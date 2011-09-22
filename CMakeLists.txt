include_directories( ${CPPUNIT_INCLUDE_DIRS} )
include_directories( ../ ${XROOTD_INCLUDES}  )
link_directories( ${XROOTD_LIB_DIR} )

add_library(
  XrdClTests SHARED
  Server.cc          Server.hh
  Utils.cc           Utils.hh
  UrlTest.cc
)

target_link_libraries( XrdClTests ${CPPUNIT_LIBRARIES} XrdCl )

add_executable( text-runner TextRunner.cc PathProcessor.hh )
target_link_libraries( text-runner dl cppunit XrdClTests )
