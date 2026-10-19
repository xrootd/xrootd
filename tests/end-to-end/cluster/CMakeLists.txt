add_bats_test(cluster.bats)
add_bats_test(full.bats)
add_bats_test(no-cmsd.bats)

# The servers set the CA of the scitokens library, which fetches the keys of
# the issuer over HTTPS, with scitoken_config_set_str
if(BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)
  add_bats_test(tokens.bats)
endif()
