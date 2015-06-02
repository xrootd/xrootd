
add_subdirectory( common )
add_subdirectory( XrdClTests )
add_subdirectory( XrdSsiTests )

if( BUILD_CEPH )
  include( XrdCephTests )
endif()
