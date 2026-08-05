add_executable(xrdcks-unit-tests
  XrdCksTest.cc
  )

target_link_libraries(xrdcks-unit-tests
  XrdCl
  )
