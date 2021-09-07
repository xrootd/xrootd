
include(XRootDCommon)

add_library(
  XrdClTestsHelper SHARED
  Server.cc Server.hh
  Utils.cc Utils.hh
  TestEnv.cc TestEnv.hh
  CppUnitXrdHelpers.hh)

target_link_libraries(
  XrdClTestsHelper
  ${CMAKE_THREAD_LIBS_INIT}
  ${CPPUNIT_LIBRARIES}
  ${ZLIB_LIBRARIES}
  XrdCl
  XrdUtils)

add_executable(
  test-runner
  TextRunner.cc
  PathProcessor.hh)

target_link_libraries(
  test-runner
  ${CMAKE_DL_LIBS}
  ${CPPUNIT_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
 TARGETS XrdClTestsHelper test-runner
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
