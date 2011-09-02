include( FindPackageHandleStandardArgs )

if( KERBEROS5_INCLUDE_DIR AND KERBEROS5_LIBRARY )
  set( KERBEROS5_FOUND TRUE )
else( KERBEROS5_INCLUDE_DIR AND KERBEROS5_LIBRARY )
  find_path( KERBEROS5_INCLUDE_DIR krb5.h
    /usr/include
    /usr/kerberos/include
    /usr/krb5/include
    /usr/local/kerberos/include
    /usr/local/include )

  find_library( KERBEROS5_LIBRARY NAMES krb5 )

  find_package_handle_standard_args(
    KERBEROS5
    DEFAULT_MSG
    KERBEROS5_LIBRARY KERBEROS5_INCLUDE_DIR )

  mark_as_advanced( KERBEROS5_INCLUDE_DIR KERBEROS5_LIBRARY )
endif( KERBEROS5_INCLUDE_DIR AND KERBEROS5_LIBRARY )
