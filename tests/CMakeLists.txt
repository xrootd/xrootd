if(NOT BUILD_TESTS)
  return()
endif()

include(GoogleTest)

add_subdirectory(common)

add_subdirectory(XrdCl)
add_subdirectory(XrdCeph)
add_subdirectory(XrdEc)

add_subdirectory(XrdHttpTests)

add_subdirectory( XrdSsiTests )

execute_process(COMMAND id -u OUTPUT_VARIABLE UID
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if(XRDCL_ONLY OR UID EQUAL 0)
  return()
endif()

add_subdirectory( XRootD )
add_subdirectory( cluster )
