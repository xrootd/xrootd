
include(XRootDCommon)

add_library(
  XrdClTestsHelper SHARED
  Server.cc Server.hh
  Utils.cc Utils.hh
  TestEnv.cc TestEnv.hh
  CppUnitXrdHelpers.hh)

target_link_libraries(
 XrdClTestsHelper
  pthread
  ${CPPUNIT_LIBRARIES}
  ${ZLIB_LIBRARY}
  XrdCl)
