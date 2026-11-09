add_executable(xrdckstest XrdCksTest.cc)

target_link_libraries(xrdckstest XrdCl)

add_bats_tests(XrdCks::on-demand cks-ondemand.bats)
add_bats_tests(XrdCks::real-time cks-realtime.bats)
