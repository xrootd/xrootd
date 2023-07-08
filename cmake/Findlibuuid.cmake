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

foreach(var FOUND INCLUDE_DIR INCLUDE_DIRS LIBRARY LIBRARIES)
  unset(UUID_${var} CACHE)
endforeach()

if(NOT UUID_INCLUDE_DIR)
  find_path(UUID_INCLUDE_DIR uuid/uuid.h)
endif()

if(IS_DIRECTORY "${UUID_INCLUDE_DIR}")
  include(CheckCXXSymbolExists)
  set(UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIR})
  set(CMAKE_REQUIRED_INCLUDES ${UUID_INCLUDE_DIRS})
  check_cxx_symbol_exists("uuid_generate_random" "uuid/uuid.h" _uuid_header_only)
  unset(CMAKE_REQUIRED_INCLUDES)
endif()

if(_uuid_header_only)
  find_package_handle_standard_args(libuuid DEFAULT_MSG UUID_INCLUDE_DIR)
else()
  if(NOT UUID_LIBRARY)
    include(CheckLibraryExists)
    check_library_exists("uuid" "uuid_generate_random" "" _have_libuuid)

    if(_have_libuuid)
      set(UUID_LIBRARY "uuid")
      set(UUID_LIBRARIES ${UUID_LIBRARY})
    else()
      find_package(PkgConfig)
      if(PKG_CONFIG_FOUND)
        if(${libuuid_FIND_REQUIRED})
          set(libuuid_REQUIRED REQUIRED)
        endif()
        pkg_check_modules(UUID ${libuuid_REQUIRED} uuid)
        set(UUID_LIBRARIES ${UUID_LDFLAGS})
        set(UUID_LIBRARY ${UUID_LIBRARIES})
        set(UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIRS})
        set(UUID_INCLUDE_DIR ${UUID_INCLUDE_DIRS})
      endif()
    endif()
    unset(_have_libuuid)
  endif()
  find_package_handle_standard_args(libuuid DEFAULT_MSG UUID_INCLUDE_DIR UUID_LIBRARY)
endif()

if(LIBUUID_FOUND AND NOT TARGET uuid::uuid)
  add_library(uuid::uuid INTERFACE IMPORTED)
  set_property(TARGET uuid::uuid PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${UUID_INCLUDE_DIRS}")
  set_property(TARGET uuid::uuid PROPERTY INTERFACE_LINK_LIBRARIES "${UUID_LIBRARIES}")
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
