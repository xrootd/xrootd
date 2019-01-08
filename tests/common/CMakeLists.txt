
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
  XrdCl
  XrdUtils)

add_executable(text-runner TextRunner.cc PathProcessor.hh)
target_link_libraries(text-runner ${CMAKE_DL_LIBS} ${CPPUNIT_LIBRARIES} pthread)

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
 TARGETS XrdClTestsHelper text-runner
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
