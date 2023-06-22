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

if(EXISTS UUID_INCLUDE_DIR)
  set(UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIR})
  set(CMAKE_REQUIRED_INCLUDES ${UUID_INCLUDE_DIRS})
  check_cxx_symbol_exists("uuid_generate_random" "uuid/uuid.h" _uuid_header_only)
  unset(CMAKE_REQUIRED_INCLUDES)
endif()

if(NOT _uuid_header_only AND NOT UUID_LIBRARY)
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

if(UUID_FOUND AND NOT TARGET uuid::uuid)
  add_library(uuid::uuid INTERFACE IMPORTED)
  set_property(TARGET uuid::uuid PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${UUID_INCLUDE_DIRS}")
  set_property(TARGET uuid::uuid PROPERTY INTERFACE_LINK_LIBRARIES "${UUID_LIBRARIES}")
endif()

if(_uuid_header_only)
  find_package_handle_standard_args(libuuid DEFAULT_MSG UUID_INCLUDE_DIR)
else()
  find_package_handle_standard_args(libuuid DEFAULT_MSG UUID_INCLUDE_DIR UUID_LIBRARY)
endif()

unset(_uuid_header_only)
mark_as_advanced(UUID_INCLUDE_DIR UUID_LIBRARY)
