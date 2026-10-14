add_bats_test(XrdCl::tpc-http-bigfiles bigfiles.bats)
add_bats_test(XrdCl::tpc-http-https https.bats)
add_bats_test(XrdCl::tpc-http-plain plain.bats)
add_bats_test(XrdCl::tpc-http-protocol protocol.bats)
add_bats_test(XrdCl::tpc-http-timeout timeout.bats)

# The progress bar is printed only to a terminal, which script(1) provides
find_program(SCRIPT_EXECUTABLE script)
if(SCRIPT_EXECUTABLE)
  add_bats_test(XrdCl::tpc-http-progressbar progressbar.bats)
endif()

if(BUILD_MACAROONS)
  add_bats_test(XrdCl::tpc-http-macaroon macaroon.bats)
  add_bats_test(XrdCl::tpc-http-token-handle token_handle.bats)
endif()

# The OAuth test uses the SciTokens module to validate the tokens
if(BUILD_SCITOKENS)
  add_bats_test(XrdCl::tpc-http-oauth oauth.bats)
  add_bats_test(XrdCl::tpc-http-scitokens scitokens.bats)
endif()
