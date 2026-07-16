#-------------------------------------------------------------------------------
# End-to-end tests driven by BATS (Bash Automated Testing System).
#
# Each .bats file is registered as its own CTest test, so CTest runs BATS once
# per file (in parallel with the rest, like every other CTest test) and reports
# each file individually. The @test cases inside a file are deliberately NOT
# registered with CTest. Which BATS formatter is used depends on whether this
# build submits to CDash:
#
#   * CDash builds (-DCDASH=ON, set by test.cmake): bats-format-cdash writes
#     every test case into Testing/BatsTest<N>.xml, in CTest's own Test.xml
#     format, which test.cmake then submits via ctest_submit alongside Test.xml.
#     Each per-file BATS run produces its own numbered BatsTest<N>.xml.
#   * otherwise: the default (tap) formatter, for readable local output.
#-------------------------------------------------------------------------------

find_program(BASH_PROGRAM bash)

if(NOT BASH_PROGRAM)
  message(WARNING "bash not found: end-to-end (BATS) tests are disabled")
  return()
endif()

set(BATS ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats)
set(BATS_LIB_PATH ${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats)

set(BATS_FORMATTER ${CMAKE_CURRENT_SOURCE_DIR}/helper/bats-format-cdash)

# BATS_LIB_PATH lets bats_load_library find the vendored bats-support and
# bats-assert helpers; PATH exposes the freshly built xrootd/xrdcp binaries.
# BATS_XML_SOURCE_ROOT and BATS_XML_TEST_ROOT are only consumed by
# bats-format-cdash: the former makes <Path> relative to the repo root, the
# latter builds each test's "end-to-end <file>" display prefix relative to this
# test tree. The output directory (BATS_XML_DIR) and the Site identity
# (BuildName/BuildStamp/Name) are inherited at run time from test.cmake, which
# knows CTEST_BINARY_DIRECTORY, so they are deliberately NOT pinned here.
set(BATS_ENVIRONMENT
  "BATS_LIB_PATH=${BATS_LIB_PATH}"
  "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
  "BATS_XML_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
  "BATS_XML_TEST_ROOT=${CMAKE_CURRENT_SOURCE_DIR}")

# Discover every .bats file and register each as its own CTest test, so BATS is
# run once per file instead of once recursively over the whole tree.
# CONFIGURE_DEPENDS re-globs at build time so newly added .bats files are picked
# up without a manual re-run of CMake.
file(GLOB_RECURSE BATS_FILES CONFIGURE_DEPENDS
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.bats)

foreach(BATS_FILE ${BATS_FILES})
  string(REGEX REPLACE "\\.bats$" "" BATS_NAME "${BATS_FILE}")
  set(BATS_TEST "end-to-end ${BATS_NAME}")

  add_test(NAME ${BATS_TEST}
    COMMAND ${BASH_PROGRAM} ${BATS} --timing --verbose-run --show-output-of-passing-tests
            --formatter ${BATS_FORMATTER} ${CMAKE_CURRENT_SOURCE_DIR}/${BATS_FILE})

  set_tests_properties(${BATS_TEST} PROPERTIES ENVIRONMENT "${BATS_ENVIRONMENT}")
endforeach()
