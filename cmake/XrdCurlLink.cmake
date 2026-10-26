include_guard(GLOBAL)

# CMake 3.18's FindCURL imported target does not carry the libraries a static
# libcurl needs. curl's own CMake package does. When the selected file is a
# static archive and the imported target has no link interface, take the
# static link line from pkg-config.
function(xrd_target_link_curl target_name)
  if(NOT TARGET CURL::libcurl)
    message(FATAL_ERROR "CURL::libcurl is not defined")
  endif()
  target_link_libraries(${target_name} PRIVATE CURL::libcurl)

  set(_loc "")
  foreach(_prop IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_DEBUG)
    if(_loc)
      break()
    endif()
    get_target_property(_try CURL::libcurl ${_prop})
    if(_try AND NOT _try MATCHES "NOTFOUND$")
      set(_loc "${_try}")
    endif()
  endforeach()
  if(NOT _loc MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    return()
  endif()

  get_target_property(_iface CURL::libcurl INTERFACE_LINK_LIBRARIES)
  if(_iface AND NOT _iface MATCHES "NOTFOUND$")
    return()
  endif()

  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    include(FindPkgConfig)
    pkg_check_modules(XRDJWT_LIBCURL QUIET libcurl)
    if(XRDJWT_LIBCURL_STATIC_LDFLAGS)
      target_link_libraries(${target_name} PRIVATE ${XRDJWT_LIBCURL_STATIC_LDFLAGS})
      return()
    endif()
  endif()

  find_package(OpenSSL REQUIRED)
  find_package(ZLIB REQUIRED)
  target_link_libraries(${target_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
endfunction()
