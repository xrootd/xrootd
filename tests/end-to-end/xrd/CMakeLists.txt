set(XRD_CLI_BATS_ENV
  "BATS_LIB_PATH=${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats"
  "DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{DYLD_LIBRARY_PATH}"
  "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}"
  "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
  "XRD=$<TARGET_FILE:xrd>"
)

add_test(NAME XrdApps::xrd-cli-read-only
  COMMAND ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats
          ${CMAKE_CURRENT_SOURCE_DIR}/xrd-cli-read-only.bats)

set_tests_properties(XrdApps::xrd-cli-read-only PROPERTIES
  ENVIRONMENT "${XRD_CLI_BATS_ENV}")
