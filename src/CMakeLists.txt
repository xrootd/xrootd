
#-------------------------------------------------------------------------------
# Include the subcomponents
#-------------------------------------------------------------------------------
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
if( XRDCEPH_SUBMODULE )
  add_compile_definitions( XRDCEPH_SUBMODULE )
endif()
include( XrdCeph )

