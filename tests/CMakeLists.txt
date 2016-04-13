
add_subdirectory( common )
add_subdirectory( XrdClTests )

if( BUILD_CEPH )
  add_subdirectory( XrdCephTests )
endif()
