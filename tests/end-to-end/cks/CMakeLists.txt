add_executable(xrdckstest XrdCksTest.cc)

target_link_libraries(xrdckstest XrdCl)

add_bats_test(ondemand.bats)
add_bats_test(realtime.bats)
