#-------------------------------------------------------------------------------
# Find the required libraries
#-------------------------------------------------------------------------------

find_package( XRootD REQUIRED )

find_package( ceph REQUIRED )

if( ENABLE_TESTS )
  find_package( CppUnit )
  if( CPPUNIT_FOUND )
    set( BUILD_TESTS TRUE )
  else()
    set( BUILD_TESTS FALSE )
  endif()
endif()
