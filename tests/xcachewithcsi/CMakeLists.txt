list(APPEND XRDENV "XRDCP=$<TARGET_FILE:xrdcp>")
list(APPEND XRDENV "XRDFS=$<TARGET_FILE:xrdfs>")
list(APPEND XRDENV "XROOTD=$<TARGET_FILE:xrootd>")
list(APPEND XRDENV "XRD_PLUGINCONFDIR=${CMAKE_SOURCE_DIR}/config/client.plugins.d")

foreach(config common srv-forxcache srv-pfc)
  configure_file(${config}.cfg ${config}.cfg @ONLY)
endforeach()

add_test(NAME XRootD::xcachewithcsi::start
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh start")
set_tests_properties(XRootD::xcachewithcsi::start
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_SETUP XRootD_xcachewithcsi)

add_test(NAME XRootD::xcachewithcsi::test
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh")
set_tests_properties(XRootD::xcachewithcsi::test
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_REQUIRED XRootD_xcachewithcsi)

add_test(NAME XRootD::xcachewithcsi::teardown
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh teardown")
set_tests_properties(XRootD::xcachewithcsi::teardown
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_CLEANUP XRootD_xcachewithcsi)
