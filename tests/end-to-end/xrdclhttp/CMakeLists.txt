if(NOT BUILD_HTTP OR NOT BUILD_SCITOKENS)
  return()
endif()

add_subdirectory(gtest)

# The servers set the CA of the scitokens library, which fetches the keys of
# the issuer over HTTPS, with scitoken_config_set_str
if(HAVE_SCITOKEN_CONFIG_SET_STR)
  add_bats_test(curl.bats)
  add_bats_test(gtest.bats)
  add_bats_test(tape.bats)
endif()
