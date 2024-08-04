#.rst:
# Findlibuuid
# -----------
#
# Find libuuid, DCE compatible Universally Unique Identifier library.
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target:
#
# ``uuid::uuid``
#   The libuuid library, if found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
# ``LIBUUID_FOUND``
#   True if libuuid has been found.
# ``UUID_INCLUDE_DIRS``
#   Where to find uuid/uuid.h.
# ``UUID_LIBRARIES``
#   The libraries to link against to use libuuid.
#
# Obsolete variables
# ^^^^^^^^^^^^^^^^^^
#
# The following variables may also be set, for backwards compatibility:
#
# ``UUID_LIBRARY``
#   where to find the libuuid library (same as UUID_LIBRARIES).
# ``UUID_INCLUDE_DIR``
#   where to find the uuid/uuid.h header (same as UUID_INCLUDE_DIRS).

include(CheckCXXSymbolExists)

if(NOT UUID_INCLUDE_DIR)
  set(CMAKE_FIND_FRAMEWORK LAST)
  find_path(UUID_INCLUDE_DIR uuid/uuid.h)
endif()

if(IS_DIRECTORY "${UUID_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_INCLUDES ${UUID_INCLUDE_DIR})
  check_cxx_symbol_exists("uuid_generate_random" "uuid/uuid.h" _uuid_header_only)
  unset(CMAKE_REQUIRED_INCLUDES)
endif()

if(NOT UUID_LIBRARY AND NOT _uuid_header_only)
  find_library(UUID_LIBRARY NAMES uuid)

  if(UUID_LIBRARY)
    set(CMAKE_REQUIRED_INCLUDES ${UUID_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${UUID_LIBRARY})
    check_cxx_symbol_exists("uuid_generate_random" "uuid/uuid.h" _have_libuuid)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
  endif()

  if(NOT _have_libuuid)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      # We need to clear cache variables set above, which pkg-config may set,
      # otherwise the call to pkg_check_modules will have no effect, as it does
      # not override cache variables.
      foreach(var FOUND INCLUDE_DIR INCLUDE_DIRS LIBRARY LIBRARIES)
        unset(UUID_${var} CACHE)
      endforeach()
      if(${libuuid_FIND_REQUIRED})
        set(libuuid_REQUIRED REQUIRED)
      endif()
      pkg_check_modules(UUID ${libuuid_REQUIRED} uuid)

      # The include directory returned by pkg-config is <prefix>/include/uuid,
      # while we expect just <prefix>/include, so strip the last component to
      # allow #include <uuid/uuid.h> to actually work.
      get_filename_component(UUID_INCLUDE_DIR ${UUID_INCLUDE_DIRS} DIRECTORY)

      set(UUID_INCLUDE_DIR ${UUID_INCLUDE_DIR} CACHE PATH "")
      set(UUID_LIBRARY ${UUID_LDFLAGS} CACHE STRING "")
      unset(UUID_INCLUDE_DIRS CACHE)
      unset(UUID_LIBRARIES CACHE)
    endif()
  endif()
endif()

if(_uuid_header_only)
  find_package_handle_standard_args(libuuid DEFAULT_MSG UUID_INCLUDE_DIR)
else()
  find_package_handle_standard_args(libuuid DEFAULT_MSG UUID_INCLUDE_DIR UUID_LIBRARY)
endif()

if(LIBUUID_FOUND)
  set(UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIR})
  set(UUID_LIBRARIES ${UUID_LIBRARY})
  if(NOT TARGET uuid::uuid)
    add_library(uuid::uuid INTERFACE IMPORTED)
    target_include_directories(uuid::uuid SYSTEM INTERFACE "${UUID_INCLUDE_DIRS}")
    target_link_libraries(uuid::uuid INTERFACE "${UUID_LIBRARIES}")
  endif()
endif()

mark_as_advanced(UUID_INCLUDE_DIR UUID_LIBRARY)

if(NOT "${libuuid_FIND_QUIET}")
  message(DEBUG "UUID_FOUND        = ${LIBUUID_FOUND}")
  message(DEBUG "UUID_HEADER_ONLY  = ${_uuid_header_only}")
  message(DEBUG "UUID_INCLUDE_DIR  = ${UUID_INCLUDE_DIR}")
  message(DEBUG "UUID_INCLUDE_DIRS = ${UUID_INCLUDE_DIRS}")
  message(DEBUG "UUID_LIBRARY      = ${UUID_LIBRARY}")
  message(DEBUG "UUID_LIBRARIES    = ${UUID_LIBRARIES}")
endif()
