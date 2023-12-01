if(XRDCL_ONLY)
  return()
endif()

execute_process(COMMAND id -u OUTPUT_VARIABLE UID OUTPUT_STRIP_TRAILING_WHITESPACE)

# ensure that we're not root
if (UID EQUAL 0)
  return()
endif()

# find executables
list(APPEND XRDENV "XRDCP=$<TARGET_FILE:xrdcp>")
list(APPEND XRDENV "XRDFS=$<TARGET_FILE:xrdfs>")
list(APPEND XRDENV "CRC32C=$<TARGET_FILE:xrdcrc32c>")
list(APPEND XRDENV "ADLER32=$<TARGET_FILE:xrdadler32>")
list(APPEND XRDENV "XROOTD=$<TARGET_FILE:xrootd>")
list(APPEND XRDENV "CMSD=$<TARGET_FILE:cmsd>")

set(SRVNAMES "metaman" "man1" "man2" "srv1" "srv2" "srv3" "srv4")

foreach(i ${SRVNAMES})
  configure_file("configs/xrootd_${i}.cfg" "configs/xrootd_${i}.cfg" @ONLY)
endforeach()



# Start the smoke test for the cluster
add_test(NAME XRootD::start::cluster
  COMMAND sh -c "cp -r ${CMAKE_CURRENT_SOURCE_DIR}/mvdata ${CMAKE_CURRENT_BINARY_DIR} && \
  ${CMAKE_CURRENT_SOURCE_DIR}/setup.sh start" )
set_tests_properties(XRootD::start::cluster PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_SETUP XRootD_Cluster)

add_test(NAME XRootD::smoke-test-cluster
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/smoketest-clustered.sh" )
set_tests_properties(XRootD::smoke-test-cluster PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_REQUIRED XRootD_Cluster)

add_test(NAME XRootD::stop::cluster
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh stop" )
set_tests_properties(XRootD::stop::cluster PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_CLEANUP XRootD_Cluster)
