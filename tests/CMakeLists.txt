if(NOT BUILD_TESTS)
  return()
endif()

include(GoogleTest)

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

add_subdirectory(unit)

add_subdirectory(common)

add_subdirectory(XrdCl)
add_subdirectory(XrdCeph)
add_subdirectory(XrdEc)
add_subdirectory(XrdPosix)

add_subdirectory(XrdHttpTests)

add_subdirectory(XrdHttpTapeApi)

add_subdirectory(XrdMacaroons)

add_subdirectory(XrdOucTests)

add_subdirectory(XrdThrottleTests)

add_subdirectory( XrdSsiTests )

add_subdirectory(XrdHttpTpc)

add_subdirectory(XrdPfcTests)

add_subdirectory(XrdXrootdTests)

add_subdirectory(XrdOssMirageTests)

if(ENABLE_SERVER_TESTS)
  add_subdirectory(XrdClHttpCommon)
endif()
add_subdirectory(XrdClHttp)

if(NOT ENABLE_SERVER_TESTS)
  return()
endif()

add_subdirectory(end-to-end)

add_subdirectory(krb5)
add_subdirectory(scitokens)
add_subdirectory(tls)

add_subdirectory(XrdClS3)

add_subdirectory( XrdOssTests )
add_subdirectory( XRootD )
add_subdirectory( cluster )
add_subdirectory( authenticated_cluster)
add_subdirectory( stress )
add_subdirectory( TPCTests )
add_subdirectory( badredir )
add_subdirectory( xcachewithcsi )
add_subdirectory(XrdSysTests)
