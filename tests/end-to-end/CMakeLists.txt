#-------------------------------------------------------------------------------
# End-to-end tests driven by BATS (Bash Automated Testing System).
#
# The whole suite is run by a single CTest test; the individual @test cases are
# deliberately NOT registered with CTest. Which BATS formatter is used depends on
# whether this build submits to CDash:
#
#   * CDash builds (-DCDASH=ON, set by test.cmake): bats-format-cdash writes
#     every test case into Testing/BatsTest.xml, in CTest's own Test.xml format,
#     which test.cmake then submits via ctest_submit alongside Test.xml.
#   * otherwise: the default (tap) formatter, for readable local output.
#-------------------------------------------------------------------------------

find_program(BASH_PROGRAM bash)

if(NOT BASH_PROGRAM)
  message(WARNING "bash not found: end-to-end (BATS) tests are disabled")
  return()
endif()

set(BATS ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats)
set(BATS_LIB_PATH ${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats)

if(CDASH)
  set(BATS_FORMATTER ${CMAKE_CURRENT_SOURCE_DIR}/helper/bats-format-cdash)
else()
  set(BATS_FORMATTER tap)
endif()

# BATS_LIB_PATH lets bats_load_library find the vendored bats-support and
# bats-assert helpers; PATH exposes the freshly built xrootd/xrdcp binaries.
# The BATS_XML_* variables are only consumed by bats-format-cdash; the Site
# identity (BuildName/BuildStamp/Name) is completed at run time by test.cmake.
set(BATS_ENVIRONMENT
  "BATS_LIB_PATH=${BATS_LIB_PATH}"
  "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
  "BATS_XML_FILE=${CMAKE_BINARY_DIR}/Testing/BatsTest.xml"
  "BATS_XML_SOURCE_ROOT=${PROJECT_SOURCE_DIR}")

add_test(NAME XRootD::end-to-end
  COMMAND ${BASH_PROGRAM} ${BATS} --recursive --timing
          --formatter ${BATS_FORMATTER} ${CMAKE_CURRENT_SOURCE_DIR})

set_tests_properties(XRootD::end-to-end PROPERTIES ENVIRONMENT "${BATS_ENVIRONMENT}")
