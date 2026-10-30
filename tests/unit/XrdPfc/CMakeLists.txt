# The tested code links XrdServer, which a client-only build does not have
if(NOT TARGET XrdServer)
  return()
endif()

# XrdPfcInfo.cc and XrdPfcDirState.cc are compiled in rather than linked from
# the plugin, as neither needs the Cache singleton.
add_executable(xrdpfc-unit-tests
  XrdPfc.cc
  XrdPfcInfo.cc
  XrdPfcDirState.cc
  ${CMAKE_SOURCE_DIR}/src/XrdPfc/XrdPfcInfo.cc
  ${CMAKE_SOURCE_DIR}/src/XrdPfc/XrdPfcDirState.cc)

target_link_libraries(xrdpfc-unit-tests XrdServer XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdpfc-unit-tests TEST_PREFIX unit::XrdPfc::
  PROPERTIES DISCOVERY_TIMEOUT 10)
