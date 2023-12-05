if(XRDCL_ONLY)
  return()
endif()

execute_process(COMMAND id -u OUTPUT_VARIABLE UID OUTPUT_STRIP_TRAILING_WHITESPACE)

if (UID EQUAL 0)
  return()
endif()

set(XRD_TEST_PORT "11940" CACHE STRING "Port for XRootD Test Server")

list(APPEND XRDENV "XRDCP=$<TARGET_FILE:xrdcp>")
list(APPEND XRDENV "XRDFS=$<TARGET_FILE:xrdfs>")
list(APPEND XRDENV "CRC32C=$<TARGET_FILE:xrdcrc32c>")
list(APPEND XRDENV "ADLER32=$<TARGET_FILE:xrdadler32>")
list(APPEND XRDENV "HOST=root://localhost:${XRD_TEST_PORT}")

configure_file(xrootd.cfg xrootd.cfg @ONLY)

add_test(NAME XRootD::start
  COMMAND sh -c "mkdir -p data && \
  $<TARGET_FILE:xrootd> -b -k fifo -n standalone -l xrootd.log -s xrootd.pid -c xrootd.cfg")
set_tests_properties(XRootD::start PROPERTIES FIXTURES_SETUP   XRootD)

add_test(NAME XRootD::stop
  COMMAND sh -c "rm -rf data && kill -s TERM $(cat standalone/xrootd.pid)")
set_tests_properties(XRootD::stop  PROPERTIES FIXTURES_CLEANUP XRootD)

add_test(NAME XRootD::smoke-test
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/smoke.sh")

set_tests_properties(XRootD::smoke-test PROPERTIES
  ENVIRONMENT "${XRDENV}" FIXTURES_REQUIRED XRootD)
