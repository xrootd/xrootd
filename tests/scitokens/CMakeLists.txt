
add_executable(xrdscitokens-create-jwks XrdScitokensCreateJwks.cc)
target_link_libraries(xrdscitokens-create-jwks PRIVATE OpenSSL::Crypto)

add_executable(xrdscitokens-create-token XrdScitokensCreateToken.cc)
target_link_libraries(xrdscitokens-create-token PRIVATE ${SCITOKENS_CPP_LIBRARIES})
target_include_directories(xrdscitokens-create-token PRIVATE ${SCITOKENS_CPP_INCLUDE_DIR})

add_test(NAME SciTokens::setup
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh ${CMAKE_BINARY_DIR}/tests/issuer")

set_tests_properties(SciTokens::setup
  PROPERTIES
    FIXTURES_SETUP SciTokens
    ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
)

add_test(NAME SciTokens::teardown
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/teardown.sh ${CMAKE_BINARY_DIR}/tests/issuer")

set_tests_properties(SciTokens::teardown
  PROPERTIES
    FIXTURES_CLEANUP SciTokens
    ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
)
