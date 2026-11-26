add_bats_tests(xroot.bats)

# GET and PUT need the HTTP server. TPC cases skip when libXrdHttpTPC is absent.
if(BUILD_HTTP)
  add_bats_tests(http.bats)
endif()
