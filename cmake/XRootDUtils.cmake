
macro( define_default variable value )
  if( NOT DEFINED ${variable} )
    set( ${variable} ${value} )
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
