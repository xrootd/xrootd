add_executable(xrdmoncollect-unit-tests
  XrdMonCollectTests.cc
  XrdMonDiskCacheTests.cc
  XrdMonOtlpTests.cc
  XrdMonShovelFrameTests.cc
  ${CMAKE_SOURCE_DIR}/src/XrdApps/XrdMonCollect/XrdMonDiskCache.cc
  ${CMAKE_SOURCE_DIR}/src/XrdApps/XrdMonCollect/XrdMonOtlp.cc)

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

  # Systemd socket activation (LISTEN_FDS): the harness plays PID 1, holds the
  # UDP socket across a collector restart, and asserts that packets sent while
  # no collector was running come out fully correlated afterwards. Only
  # meaningful when built with libsystemd (sd_listen_fds).
  if(SYSTEMD_FOUND)
    add_test(NAME XrdMonCollect::SocketActivation
      COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/socket_activation_test.sh
              $<TARGET_FILE:xrdmoncollect>)
    set_tests_properties(XrdMonCollect::SocketActivation
      PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
  endif()
endif()
