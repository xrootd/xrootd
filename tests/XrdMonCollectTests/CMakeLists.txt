add_executable(xrdmoncollect-unit-tests XrdMonCollectTests.cc)

target_link_libraries(xrdmoncollect-unit-tests
  XrdAppUtils
  XrdUtils
  GTest::gtest
  GTest::gtest_main)

gtest_discover_tests(xrdmoncollect-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)

# Configuration-file behaviour of the xrdmoncollect executable (built only when
# not XRDCL_ONLY). A small shell driver runs the binary against temporary INI
# files, checking deterministic, non-binding paths (value applied, CLI override,
# malformed/missing file errors).
if(TARGET xrdmoncollect)
  add_test(NAME XrdMonCollect::ConfigFile
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/config_test.sh
            $<TARGET_FILE:xrdmoncollect>)
endif()
