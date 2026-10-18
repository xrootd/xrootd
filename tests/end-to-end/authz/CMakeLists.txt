add_bats_test(client-cert.bats)

if(BUILD_MACAROONS)
  add_bats_test(macaroons.bats)
endif()

# The server sets the CA of the scitokens library, which fetches the keys of
# the issuer over HTTPS, with scitoken_config_set_str
if(BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)
  add_bats_test(scitokens.bats)
endif()
