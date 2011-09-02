
#-------------------------------------------------------------------------------
# Add a compiler define flag if a variable is defined
#-------------------------------------------------------------------------------
function( compiler_define_if_found predicate name )
  if( ${predicate} )
    add_definitions( -D${name} )
  endif()
endfunction( compiler_define_if_found )


#-------------------------------------------------------------------------------
#macro( check_function_exists_in_lib func lib variable )
#  set( OLD_LIBDIR ${CMAKE_REQUIRED_LIBRARIES} )
#  set( CMAKE_REQUIRED_LIBRARIES ${lib} )
#  check_function_exists( ${func} ${variable} )
#  set( CMAKE_REQUIRED_LIBRARIES ${OLD_LIBDIR} )
#endmacro( check_function_exists_in_lib )
