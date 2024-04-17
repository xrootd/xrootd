
#-------------------------------------------------------------------------------
# Add a compiler define flag if a variable is defined
#-------------------------------------------------------------------------------
macro( define_default variable value )
  if( NOT DEFINED ${variable} )
    set( ${variable} ${value} )
  endif()
endmacro()

macro( component_status name flag found )
  if( ${flag} AND ${found} )
    set( STATUS_${name} "yes" )
  elseif( ${flag} AND NOT ${found} )
    set( STATUS_${name} "libs not found" )
  else()
    set( STATUS_${name} "disabled" )
  endif()
endmacro()

#-------------------------------------------------------------------------------
# Detect in source builds
#-------------------------------------------------------------------------------
function( CheckBuildDirectory )

  # Get Real Paths of the source and binary directories
  get_filename_component( srcdir "${CMAKE_SOURCE_DIR}" REALPATH )
  get_filename_component( bindir "${CMAKE_BINARY_DIR}" REALPATH )

  # Check for in-source builds
  if( ${srcdir} STREQUAL ${bindir} )
    message( FATAL_ERROR "XRootD cannot be built in-source! "
                         "Please run cmake <src-dir> outside the "
                         "source directory and be sure to remove "
                         "CMakeCache.txt or CMakeFiles if they "
                         "exist in the source directory." )
  endif()

endfunction()
