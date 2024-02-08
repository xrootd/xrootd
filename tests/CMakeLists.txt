if(NOT BUILD_TESTS)
  return()
endif()

include(GoogleTest)
add_subdirectory( XrdCl )
add_subdirectory(XrdHttpTests)

add_subdirectory( common )
add_subdirectory( XrdClTests )
add_subdirectory( XrdSsiTests )

if( BUILD_XRDEC )
  add_subdirectory( XrdEcTests )
  add_subdirectory( XrdEc ) # new tests with GTest
endif()

add_subdirectory( XRootD )
add_subdirectory( cluster )
