add_bats_tests(xroot.bats)
add_bats_tests(xroot-tpc.bats)
set_property(TEST end-to-end.scitags.xroot end-to-end.scitags.xroot-tpc
  APPEND PROPERTY RESOURCE_LOCK "${PMARK_PEER_LOCK}")

# GET and PUT need the HTTP server. TPC cases skip when libXrdHttpTPC is absent.
if(BUILD_HTTP)
  add_bats_tests(http.bats)
  set_property(TEST end-to-end.scitags.http
    APPEND PROPERTY RESOURCE_LOCK "${PMARK_PEER_LOCK}")
endif()
