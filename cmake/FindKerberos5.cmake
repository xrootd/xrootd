include( FindPackageHandleStandardArgs )

if( KERBEROS5_INCLUDE_DIR AND KERBEROS5_LIBRARIES )
  set( KERBEROS5_FOUND TRUE )
else()
  find_path(
    KERBEROS5_INCLUDE_DIR
    NAMES krb5.h
    HINTS
    ${KERBEROS5_ROOT_DIR}
    PATH_SUFFIXES
    include )

  find_library(
    KERBEROS5_LIBRARY
    NAMES krb5
    HINTS
    ${KERBEROS5_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  find_library(
    COM_ERR_LIBRARY
    NAMES com_err
    HINTS
    ${KERBEROS5_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  set( KERBEROS5_LIBRARIES ${KERBEROS5_LIBRARY} ${COM_ERR_LIBRARY} )

  find_package_handle_standard_args(
    Kerberos5
    DEFAULT_MSG
    KERBEROS5_LIBRARIES KERBEROS5_INCLUDE_DIR )

  mark_as_advanced( KERBEROS5_INCLUDE_DIR KERBEROS5_LIBRARIES )
endif()
