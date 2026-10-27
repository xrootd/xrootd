add_bats_tests(gsi.bats)
add_bats_tests(host.bats)

if(BUILD_KRB5 AND NOT APPLE)
  add_bats_tests(krb5.bats)
endif()

add_bats_tests(sss.bats)
