add_bats_test(XrdCl::tpc-http-bigfiles bigfiles.bats)
add_bats_test(XrdCl::tpc-http-https https.bats)

if(BUILD_MACAROONS)
    add_bats_test(XrdCl::tpc-http-macaroon macaroon.bats)
endif()

add_bats_test(XrdCl::tpc-http-oauth oauth.bats)
add_bats_test(XrdCl::tpc-http-plain plain.bats)
add_bats_test(XrdCl::tpc-http-progressbar progressbar.bats)
add_bats_test(XrdCl::tpc-http-protocol protocol.bats)

if(BUILD_SCITOKENS)
    add_bats_test(XrdCl::tpc-http-scitokens scitokens.bats)
endif()

add_bats_test(XrdCl::tpc-http-timeout timeout.bats)

if(BUILD_MACAROONS)
    add_bats_test(XrdCl::tpc-http-token-handle token_handle.bats)
endif()
