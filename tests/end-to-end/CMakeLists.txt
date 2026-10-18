# add_bats_test(<script>) names the test end-to-end::<subdir>::<name> for the
# file tests/end-to-end/<subdir>/<name>.bats, with :: for each / in <subdir>.
# add_bats_test(<name> <script>) gives the name explicitly.
function(add_bats_test)
  if(ARGC EQUAL 1)
    set(script ${ARGV0})
    file(RELATIVE_PATH subdir ${PROJECT_SOURCE_DIR}/tests/end-to-end ${CMAKE_CURRENT_SOURCE_DIR})
    string(REPLACE "/" "::" subdir "${subdir}")
    get_filename_component(stem ${script} NAME_WE)
    set(name "end-to-end::${subdir}::${stem}")
  else()
    set(name ${ARGV0})
    set(script ${ARGV1})
  endif()

  set(environment
    "BATS_LIB_PATH=${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats"
    "BINARY_DIR=${CMAKE_BINARY_DIR}"
  )

  add_test(NAME ${name}
    COMMAND ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats --setup-suite-file ${PROJECT_SOURCE_DIR}/tests/end-to-end/setup_suite.bash
            ${CMAKE_CURRENT_SOURCE_DIR}/${script})

  set_tests_properties(${name} PROPERTIES ENVIRONMENT
    "${environment}")
endfunction()

add_subdirectory(setup)

add_subdirectory(cks)
add_subdirectory(http)
add_subdirectory(tpc)
add_subdirectory(mirage)
add_subdirectory(xrdfs)
