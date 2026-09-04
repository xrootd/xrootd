add_executable(xrdckstest XrdCksTest.cc)

target_link_libraries(xrdckstest XrdCl)

add_bats_test(XrdCks::on-demand cks-ondemand.bats)
add_bats_test(XrdCks::real-time cks-realtime.bats)
