#-------------------------------------------------------------------------------
# Print the configuration summary
#-------------------------------------------------------------------------------
component_status( READLINE ENABLE_READLINE READLINE_FOUND )
component_status( FUSE     BUILD_FUSE      FUSE_FOUND )
component_status( CRYPTO   BUILD_CRYPTO    OPENSSL_FOUND )
component_status( KRB5     BUILD_KRB5      KERBEROS5_FOUND )
component_status( PERL     BUILD_PERL      PERLLIBS_FOUND )

message( STATUS "----------------------------------------" )
message( STATUS "Installation path: " ${CMAKE_INSTALL_PREFIX} )
message( STATUS "C Compiler:        " ${CMAKE_C_COMPILER} )
message( STATUS "C++ Compiler:      " ${CMAKE_CXX_COMPILER} )
message( STATUS "" )
message( STATUS "Readline support:  " ${STATUS_READLINE} )
message( STATUS "Fuse support:      " ${STATUS_FUSE} )
message( STATUS "Crypto support:    " ${STATUS_CRYPTO} )
message( STATUS "Kerberos5 support: " ${STATUS_KRB5} )
message( STATUS "Perl support:      " ${STATUS_PERL} )
message( STATUS "----------------------------------------" )