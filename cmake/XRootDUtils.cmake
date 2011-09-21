
#-------------------------------------------------------------------------------
# Add a compiler define flag if a variable is defined
#-------------------------------------------------------------------------------
function( compiler_define_if_found predicate name )
  if( ${predicate} )
    add_definitions( -D${name} )
  endif()
endfunction()

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