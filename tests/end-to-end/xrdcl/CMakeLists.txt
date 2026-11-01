add_executable(xrdcl-api XrdClApi.cc)
target_link_libraries(xrdcl-api XrdCl XrdUtils Threads::Threads)

add_bats_tests(api.bats)
add_bats_tests(fork.bats)
add_bats_tests(transport.bats)
