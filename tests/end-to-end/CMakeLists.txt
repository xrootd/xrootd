#-------------------------------------------------------------------------------
# End-to-end tests driven by BATS (Bash Automated Testing System).
#
# Every *.bats file under this directory is registered as an individual CTest
# test. The tests run with the BATS interpreter vendored in vendor/bats and
# exercise the server and client binaries from the build tree.
#-------------------------------------------------------------------------------

find_program(BASH_PROGRAM bash)

if(NOT BASH_PROGRAM)
  message(WARNING "bash not found: end-to-end (BATS) tests are disabled")
  return()
endif()

set(BATS ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats)
set(BATS_LIB_PATH ${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats)

file(GLOB_RECURSE BATS_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.bats)

foreach(bats_file ${BATS_FILES})
  file(RELATIVE_PATH relative_path ${CMAKE_CURRENT_SOURCE_DIR} ${bats_file})
  string(REGEX REPLACE "\\.bats$" "" test_name "${relative_path}")
  string(REPLACE "/" "::" test_name "${test_name}")

  add_test(NAME XRootD::end-to-end::${test_name}
    COMMAND ${BASH_PROGRAM} ${BATS} --formatter tap --timing ${bats_file})

  # BATS_LIB_PATH lets bats_load_library find the vendored bats-support and
  # bats-assert helpers; PATH exposes the freshly built xrootd/xrdcp binaries.
  set_tests_properties(XRootD::end-to-end::${test_name} PROPERTIES
    ENVIRONMENT "BATS_LIB_PATH=${BATS_LIB_PATH};PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}")
endforeach()
