# The FUSE tests need a FUSE device and permission to mount, thus they are
# optional.
if(ENABLE_FUSE_TESTS)
  add_bats_test(fuse.bats)
endif()
