set(XRDFS_OPTIONS_BATS_ENV
  "BATS_LIB_PATH=${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats"
  "DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{DYLD_LIBRARY_PATH}"
  "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}"
  "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
  "XRDFS=$<TARGET_FILE:xrdfs>"
)

add_test(NAME XrdCl::xrdfs-options
  COMMAND ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats
          ${CMAKE_CURRENT_SOURCE_DIR}/xrdfs-options.bats)

set_tests_properties(XrdCl::xrdfs-options
  PROPERTIES ENVIRONMENT "${XRDFS_OPTIONS_BATS_ENV}")
