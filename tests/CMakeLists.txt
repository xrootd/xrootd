
add_subdirectory( common )
add_subdirectory( XrdClTests )

if( BUILD_CEPH )
  include( XrdCephTests )
endif()
