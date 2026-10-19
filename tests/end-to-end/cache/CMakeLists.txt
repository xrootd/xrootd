# Range-reading client of the cache tests. xrdcp reads only whole files, but
# the cache works in blocks. Threads::Threads, as the mode with concurrent
# readers uses std::thread, which needs pthread linked explicitly on glibc
# older than 2.34.
add_executable(xrdpfc-read xrdpfc-read.cc)
target_link_libraries(xrdpfc-read XrdCl XrdUtils Threads::Threads)

add_bats_test(read.bats)
add_bats_test(blacklist.bats)
add_bats_test(checksum.bats)
add_bats_test(evict.bats)
add_bats_test(http.bats)
add_bats_test(prefetch.bats)
add_bats_test(purge.bats)
add_bats_test(restart.bats)
add_bats_test(spaces.bats)

# The purge runs each 60 s, and the test must wait for it.
set_tests_properties(end-to-end::cache::purge PROPERTIES TIMEOUT 600 LABELS slow)
