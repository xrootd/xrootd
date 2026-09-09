# XrdPfcInfo.cc is compiled in rather than linked from the plugin: the cinfo
# reader has no dependency on the Cache singleton (xrdpfc_print builds it the
# same way), so the format can be tested without standing up a cache.
add_executable(xrdpfc-unit-tests
  XrdPfcTests.cc
  XrdPfcInfoTests.cc
  ${CMAKE_SOURCE_DIR}/src/XrdPfc/XrdPfcInfo.cc)

target_link_libraries(xrdpfc-unit-tests XrdServer XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdpfc-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
