add_bats_tests(xroot.bats)
set_property(TEST end-to-end.scitags.xroot
  APPEND PROPERTY RESOURCE_LOCK "${PMARK_PEER_LOCK}")

# GET and PUT need the HTTP server.
if(BUILD_HTTP)
  add_bats_tests(http.bats)
  set_property(TEST end-to-end.scitags.http
    APPEND PROPERTY RESOURCE_LOCK "${PMARK_PEER_LOCK}")
endif()
