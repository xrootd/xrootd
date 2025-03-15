
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

#-------------------------------------------------------------------------------
# Detect what kind of solaris machine we're running
#-------------------------------------------------------------------------------
macro( define_solaris_flavor )
  execute_process( COMMAND isainfo
                   OUTPUT_VARIABLE SOLARIS_ARCH )
  string( REPLACE " " ";" SOLARIS_ARCH_LIST ${SOLARIS_ARCH} )

  # amd64 (opteron)
  list( FIND SOLARIS_ARCH_LIST amd64 SOLARIS_AMD64 )
  if( SOLARIS_AMD64 EQUAL -1 )
    set( SOLARIS_AMD64 FALSE )
  else()
    set( SOLARIS_AMD64 TRUE )
  endif()

endmacro()
