cmake_host_system_information(RESULT HOSTNAME QUERY FQDN)

configure_file(tls.conf tls.conf @ONLY)

add_test(NAME TLS::setup
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/tls.sh setup")

add_test(NAME TLS::teardown
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/tls.sh teardown")

set_tests_properties(TLS::setup    PROPERTIES FIXTURES_SETUP   TLS)
set_tests_properties(TLS::teardown PROPERTIES FIXTURES_CLEANUP TLS)
