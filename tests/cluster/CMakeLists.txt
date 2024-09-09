list(APPEND XRDENV "XRDCP=$<TARGET_FILE:xrdcp>")
list(APPEND XRDENV "XRDFS=$<TARGET_FILE:xrdfs>")
list(APPEND XRDENV "CRC32C=$<TARGET_FILE:xrdcrc32c>")
list(APPEND XRDENV "ADLER32=$<TARGET_FILE:xrdadler32>")
list(APPEND XRDENV "XROOTD=$<TARGET_FILE:xrootd>")
list(APPEND XRDENV "CMSD=$<TARGET_FILE:cmsd>")

foreach(config common metaman man1 man2 srv1 srv2 srv3 srv4)
  configure_file(${config}.cfg ${config}.cfg @ONLY)
endforeach()

file(COPY mvdata DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME XRootD::cluster::start
  COMMAND sh -c "cp -r ${CMAKE_CURRENT_SOURCE_DIR}/mvdata ${CMAKE_CURRENT_BINARY_DIR} \
    && ${CMAKE_CURRENT_SOURCE_DIR}/setup.sh start")
set_tests_properties(XRootD::cluster::start
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_SETUP XRootD_Cluster)

add_test(NAME XRootD::cluster::test
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh")
set_tests_properties(XRootD::cluster::test
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_REQUIRED XRootD_Cluster)

add_test(NAME XRootD::cluster::stop
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh stop")
set_tests_properties(XRootD::cluster::stop
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_CLEANUP XRootD_Cluster)
