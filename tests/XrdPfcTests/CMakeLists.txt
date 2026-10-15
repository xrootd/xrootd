# XrdPfcInfo.cc and XrdPfcDirState.cc are compiled in rather than linked from
# the plugin, as neither needs the Cache singleton.
add_executable(xrdpfc-unit-tests
  XrdPfcTests.cc
  XrdPfcInfoTests.cc
  XrdPfcDirStateTests.cc
  ${CMAKE_SOURCE_DIR}/src/XrdPfc/XrdPfcInfo.cc
  ${CMAKE_SOURCE_DIR}/src/XrdPfc/XrdPfcDirState.cc)

target_link_libraries(xrdpfc-unit-tests XrdServer XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdpfc-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
