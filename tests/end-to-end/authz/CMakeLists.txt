add_bats_tests(client-cert.bats)

add_library(XrdOfsPrepPathAuth-${PLUGIN_VERSION} MODULE PrepPathAuth.cc)
target_link_libraries(XrdOfsPrepPathAuth-${PLUGIN_VERSION} PRIVATE XrdUtils)
add_bats_tests(prepare.bats)

if(BUILD_MACAROONS)
  add_bats_tests(macaroons.bats)
endif()

# The server sets the CA of the scitokens library, which fetches the keys of
# the issuer over HTTPS, with scitoken_config_set_str
if(BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)
  add_bats_tests(scitokens.bats)
endif()
