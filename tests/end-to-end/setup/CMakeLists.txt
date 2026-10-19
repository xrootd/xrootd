# The tools of the test issuer: xrdscitokens-create-jwks writes the public key
# of the shared credentials as a JWKS (see credentials.sh), and
# xrdscitokens-create-token makes the tokens of the tests (make_token).
if(BUILD_SCITOKENS)
  add_executable(xrdscitokens-create-jwks XrdScitokensCreateJwks.cc)
  target_link_libraries(xrdscitokens-create-jwks PRIVATE OpenSSL::Crypto)

  add_executable(xrdscitokens-create-token XrdScitokensCreateToken.cc)
  target_link_libraries(xrdscitokens-create-token PRIVATE ${SCITOKENS_CPP_LIBRARIES})
  target_include_directories(xrdscitokens-create-token PRIVATE ${SCITOKENS_CPP_INCLUDE_DIR})
endif()

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
