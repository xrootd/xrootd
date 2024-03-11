foreach(var LIBZIP_FOUND LIBZIP_INCLUDE_DIR LIBZIP_LIBZIP_LIBRARY LIBZIP_LIBRARIES)
  unset(${var} CACHE)
endforeach()

include( FindPackageHandleStandardArgs )

if( LIBZIP_INCLUDE_DIRS AND LIBZIP_LIBRARIES )
  set( LIBZIP_FOUND TRUE )
else()

  find_path(
    LIBZIP_INCLUDE_DIR
    NAMES zip.h zipconf.h
    HINTS
    ${LIBZIP_ROOT_DIR}
    PATH_SUFFIXES
    include )
  set( LIBZIP_INCLUDE_DIRS ${LIBZIP_INCLUDE_DIR})

  find_library(
    LIBZIP_LIBRARY
    NAMES zip
    HINTS
    ${LIBZIP_ROOT_DIR}
    PATH_SUFFIXES lib64
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})
  set( LIBZIP_LIBRARIES ${LIBZIP_LIBRARY} )

  find_package_handle_standard_args(
    libzip
    DEFAULT_MSG
    LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR )

   mark_as_advanced( LIBZIP_INCLUDE_DIR LIBZIP_LIBRARY )
endif()
