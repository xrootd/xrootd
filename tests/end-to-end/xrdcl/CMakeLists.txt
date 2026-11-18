add_subdirectory(gtest)

add_executable(xrdcl-api XrdClApi.cc)
target_link_libraries(xrdcl-api XrdCl XrdUtils Threads::Threads)

add_bats_tests(api.bats)
add_bats_tests(cluster.bats)
add_bats_tests(fork.bats)
add_bats_tests(transport.bats)

# One file runs all tests of xrdcl-cluster-tests, after the start of the
# cluster and the creation of 2 GiB of test data, thus it can take longer
# than the default limit of 300 s on a slow host.
set_tests_properties(end-to-end.xrdcl.cluster PROPERTIES TIMEOUT 1200 LABELS slow)
