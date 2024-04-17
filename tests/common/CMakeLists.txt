

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
  ZLIB::ZLIB
  XrdCl
  XrdUtils)

target_include_directories(
  XrdClTestsHelper PUBLIC ${CPPUNIT_INCLUDE_DIRS})

add_executable(
  test-runner
  TextRunner.cc
  PathProcessor.hh)

target_link_libraries(
  test-runner
  ${CMAKE_DL_LIBS}
  ${CPPUNIT_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(
  test-runner PRIVATE ${CPPUNIT_INCLUDE_DIRS})

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
 TARGETS XrdClTestsHelper test-runner
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
