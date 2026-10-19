# The tests use the XrdClHttp plugin, which only exists when libcurl was found
if(NOT BUILD_HTTP OR NOT BUILD_SCITOKENS OR NOT TARGET XrdClHttpObj)
  return()
endif()

add_subdirectory(gtest)

# The servers set the CA of the scitokens library, which fetches the keys of
# the issuer over HTTPS, with scitoken_config_set_str
if(HAVE_SCITOKEN_CONFIG_SET_STR)
  add_bats_tests(curl.bats)
  add_bats_tests(gtest.bats)
  add_bats_tests(tape.bats)
endif()
