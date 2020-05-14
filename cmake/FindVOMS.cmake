include( FindPackageHandleStandardArgs )

if( VOMS_INCLUDE_DIRS AND VOMS_LIBRARIES )
  set( VOMS_FOUND TRUE )
else()

  find_path(
    VOMS_INCLUDE_DIR
    NAMES voms/voms_api.h
    HINTS
    ${VOMS_ROOT_DIR}
    PATH_SUFFIXES
    include )
  set( VOMS_INCLUDE_DIRS ${VOMS_INCLUDE_DIR})

  find_library(
    VOMS_LIBRARY
    NAMES vomsapi
    HINTS
    ${VOMS_ROOT_DIR}
    PATH_SUFFIXES lib64
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})
  set( VOMS_LIBRARIES ${VOMS_LIBRARY} )

  find_package_handle_standard_args(
    VOMS
    DEFAULT_MSG
    VOMS_LIBRARY VOMS_INCLUDE_DIR )

   mark_as_advanced( VOMS_INCLUDE_DIR VOMS_LIBRARY )
endif()
