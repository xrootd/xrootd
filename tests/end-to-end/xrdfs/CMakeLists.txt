add_bats_test(XrdCl::xrdfs-commands xrdfs-commands.bats)
add_bats_test(XrdCl::xrdfs-full-url xrdfs-full-url.bats)
add_bats_test(XrdCl::xrdfs-gfal2-reference xrdfs-gfal2-reference.bats)

set_tests_properties(XrdCl::xrdfs-gfal2-reference
  PROPERTIES
    LABELS reference
    TIMEOUT 300)
