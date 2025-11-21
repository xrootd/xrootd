if(APPLE)
  return()
endif()

configure_file(kdc.conf kdc.conf @ONLY)
configure_file(krb5.conf krb5.conf @ONLY)

file(COPY kdc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME Kerberos::setup
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/kerberos.sh setup")

add_test(NAME Kerberos::teardown
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/kerberos.sh teardown")

set_tests_properties(Kerberos::setup    PROPERTIES FIXTURES_SETUP   Kerberos)
set_tests_properties(Kerberos::teardown PROPERTIES FIXTURES_CLEANUP Kerberos)
