
macro( define_default variable value )
  if( NOT DEFINED ${variable} )
    set( ${variable} ${value} )
  endif()
endmacro()
