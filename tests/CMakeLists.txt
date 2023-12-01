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

if( BUILD_CEPH )
  add_subdirectory( XrdCephTests )
endif()

add_subdirectory( XRootD )
add_subdirectory( cluster )
