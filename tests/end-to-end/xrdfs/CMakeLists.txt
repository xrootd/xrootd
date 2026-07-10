set(XRDFS_BATS_ENV
  "BATS_LIB_PATH=${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats"
  "DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{DYLD_LIBRARY_PATH}"
  "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}"
  "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
  "XRDCP=$<TARGET_FILE:xrdcp>"
  "XRDFS=$<TARGET_FILE:xrdfs>"
)

add_test(NAME XrdCl::xrdfs-full-url
  COMMAND ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats
          ${CMAKE_CURRENT_SOURCE_DIR}/xrdfs-full-url.bats)

set_tests_properties(XrdCl::xrdfs-full-url
  PROPERTIES ENVIRONMENT "${XRDFS_BATS_ENV}")
