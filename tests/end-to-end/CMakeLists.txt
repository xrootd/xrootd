function(add_bats_test name script)
  set(environment
    "BATS_LIB_PATH=${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats"
    "BINARY_DIR=${CMAKE_BINARY_DIR}"
  )

  add_test(NAME ${name}
    COMMAND ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats
            ${CMAKE_CURRENT_SOURCE_DIR}/${script})

  set_tests_properties(${name} PROPERTIES ENVIRONMENT
    "${environment}")
endfunction()

add_subdirectory(xrdfs)
