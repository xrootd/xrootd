include( FindPackageHandleStandardArgs )

if( BONJOUR_INCLUDE_DIR AND BONJOUR_LIBRARIES )
  set( BONJOUR_FOUND TRUE )
else()
  find_path(
    BONJOUR_INCLUDE_DIR
    NAMES dns_sd.h
    HINTS
    ${BONJOUR_ROOT_DIR}
    PATH_SUFFIXES
    include )

if(Linux)
  find_library(
    BONJOUR_LIBRARY
    NAMES dns_sd
    HINTS
    ${BONJOUR_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  find_library(
    AVAHI_CLIENT_LIBRARY
    NAMES avahi-client
    HINTS
    ${BONJOUR_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  find_library(
    AVAHI_COMMON_LIBRARY
    NAMES avahi-common
    HINTS
    ${BONJOUR_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  set( BONJOUR_LIBRARIES ${BONJOUR_LIBRARY} ${AVAHI_CLIENT_LIBRARY} ${AVAHI_COMMON_LIBRARY} )
endif()

if(MacOSX)
  set( BONJOUR_LIBRARIES System )
endif()

  find_package_handle_standard_args(
    BONJOUR
    DEFAULT_MSG
    BONJOUR_LIBRARIES BONJOUR_INCLUDE_DIR )

  mark_as_advanced( BONJOUR_INCLUDE_DIR BONJOUR_LIBRARIES )
endif()
