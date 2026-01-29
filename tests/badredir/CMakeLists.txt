list(APPEND XRDENV "XRDCP=$<TARGET_FILE:xrdcp>")
list(APPEND XRDENV "XRDFS=$<TARGET_FILE:xrdfs>")
list(APPEND XRDENV "XROOTD=$<TARGET_FILE:xrootd>")
list(APPEND XRDENV "XRD_PLUGINCONFDIR=${CMAKE_SOURCE_DIR}/config/client.plugins.d")

foreach(config common srv-bad-redir1 srv-bad-redir2)
  configure_file(${config}.cfg ${config}.cfg @ONLY)
endforeach()

add_test(NAME XRootD::badredir::start
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh start")
set_tests_properties(XRootD::badredir::start
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_SETUP XRootD_Badredir)

add_test(NAME XRootD::badredir::test
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh")
set_tests_properties(XRootD::badredir::test
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_REQUIRED XRootD_Badredir)

add_test(NAME XRootD::badredir::teardown
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh teardown")
set_tests_properties(XRootD::badredir::teardown
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_CLEANUP XRootD_Badredir)
