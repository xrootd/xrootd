find_program(BASH_PROGRAM bash)

if(NOT BASH_PROGRAM)
  message(WARNING "bash not found: end-to-end (BATS) tests are disabled")
  return()
endif()

set(BATS ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats)
set(BATS_LIB_PATH ${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats)

set(BATS_FORMATTER ${CMAKE_CURRENT_SOURCE_DIR}/helper/bats-format-cdash)

file(STRINGS "${CMAKE_BINARY_DIR}/Testing/TAG" BATS_BUILD_TAG LIMIT_COUNT 1)
set(BATS_REPORT_DIRECTORY "${CMAKE_BINARY_DIR}/Testing/${BATS_BUILD_TAG}")

set(BATS_ENVIRONMENT
  "BATS_LIB_PATH=${BATS_LIB_PATH}"
  "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}")

file(GLOB_RECURSE BATS_FILES CONFIGURE_DEPENDS
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.bats)

foreach(BATS_FILE ${BATS_FILES})
  string(REGEX REPLACE "\\.bats$" "" BATS_NAME "${BATS_FILE}")
  set(BATS_TEST "e2e ${BATS_NAME}")

  add_test(NAME ${BATS_TEST}
    COMMAND ${BASH_PROGRAM} -c "${BATS} --timing --verbose-run --show-output-of-passing-tests \
      --formatter ${BATS_FORMATTER} ${CMAKE_CURRENT_SOURCE_DIR}/${BATS_FILE} \
      > $(mktemp -p ${BATS_REPORT_DIRECTORY} -t 'BatsTest.XXX.xml')" )

  set_tests_properties(${BATS_TEST} PROPERTIES ENVIRONMENT "${BATS_ENVIRONMENT}")
endforeach()
