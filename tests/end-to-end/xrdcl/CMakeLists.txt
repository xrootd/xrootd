add_executable(xrdcl-api XrdClApi.cc)
target_link_libraries(xrdcl-api XrdCl XrdUtils Threads::Threads)

add_bats_test(api.bats)
add_bats_test(connection.bats)
add_bats_test(fork.bats)
add_bats_test(login.bats)
add_bats_test(redirect.bats)
add_bats_test(transport.bats)
