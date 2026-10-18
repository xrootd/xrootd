add_bats_tests(bigfiles.bats)
add_bats_tests(connections.bats)
add_bats_tests(https.bats)
add_bats_tests(plain.bats)
add_bats_tests(protocol.bats)
add_bats_tests(timeout.bats)

# The progress bar is printed only to a terminal, which script(1) provides
find_program(SCRIPT_EXECUTABLE script)
if(SCRIPT_EXECUTABLE)
  add_bats_tests(progressbar.bats)
endif()

if(BUILD_MACAROONS)
  add_bats_tests(macaroon.bats)
  add_bats_tests(token_handle.bats)
endif()

# The OAuth test uses the SciTokens module to validate the tokens
if(BUILD_SCITOKENS)
  add_bats_tests(oauth.bats)
endif()

# The servers set the CA of the scitokens library, which fetches the keys of
# the issuer over HTTPS, with scitoken_config_set_str
if(BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)
  add_bats_tests(scitokens.bats)
endif()
