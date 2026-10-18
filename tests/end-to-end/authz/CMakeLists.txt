add_bats_tests(client-cert.bats)

if(BUILD_MACAROONS)
  add_bats_tests(macaroons.bats)
endif()
