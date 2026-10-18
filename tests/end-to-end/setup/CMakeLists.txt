add_test(NAME end-to-end::setup::credentials::create
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/credentials.sh create ${CREDENTIALS_DIR})

add_test(NAME end-to-end::setup::credentials::remove
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/credentials.sh remove ${CREDENTIALS_DIR})

set_tests_properties(end-to-end::setup::credentials::create PROPERTIES
  FIXTURES_SETUP credentials
  ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}")

set_tests_properties(end-to-end::setup::credentials::remove PROPERTIES
  FIXTURES_CLEANUP credentials)

add_bats_test(setup.bats)
