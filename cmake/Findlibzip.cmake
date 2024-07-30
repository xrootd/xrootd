#.rst:
# Findlibzip
# -------
#
# Find libzip library for file management over HTTP-based protocols.
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target:
#
# ``libzip::zip``
#   The libzip library, if found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
# ``LIBZIP_FOUND``
#   True if libzip has been found.
# ``LIBZIP_INCLUDE_DIRS``
#   Where to find libzip.hpp, etc.
# ``LIBZIP_LIBRARIES``
#   The libraries to link against to use libzip.
# ``LIBZIP_VERSION``
#   The version of the libzip library found (e.g. 1.9.2).
#
# Obsolete variables
# ^^^^^^^^^^^^^^^^^^
#
# The following variables may also be set, for backwards compatibility:
#
# ``LIBZIP_LIBRARY``
#   where to find the LIBZIP library.
# ``LIBZIP_INCLUDE_DIR``
#   where to find the LIBZIP headers (same as LIBZIP_INCLUDE_DIRS)
#

foreach(var FOUND INCLUDE_DIR INCLUDE_DIRS LIBRARY LIBRARIES)
  unset(LIBZIP_${var} CACHE)
endforeach()

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
  if(${libzip_FIND_REQUIRED})
    set(libzip_REQUIRED REQUIRED)
  endif()

  if(NOT DEFINED libzip_FIND_VERSION)
    pkg_check_modules(LIBZIP ${libzip_REQUIRED} libzip)
  else()
    pkg_check_modules(LIBZIP ${libzip_REQUIRED} libzip>=${libzip_FIND_VERSION})
  endif()

  set(LIBZIP_LIBRARIES ${LIBZIP_LDFLAGS})
  set(LIBZIP_LIBRARY ${LIBZIP_LIBRARIES})
  set(LIBZIP_INCLUDE_DIRS ${LIBZIP_INCLUDE_DIRS})
  set(LIBZIP_INCLUDE_DIR ${LIBZIP_INCLUDE_DIRS})
endif()

if(LIBZIP_FOUND AND NOT TARGET libzip::zip)
  add_library(libzip::zip INTERFACE IMPORTED)
  set_property(TARGET libzip::zip PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIRS}")
  set_property(TARGET libzip::zip PROPERTY INTERFACE_LINK_LIBRARIES "${LIBZIP_LIBRARIES}")
endif()

mark_as_advanced(LIBZIP_INCLUDE_DIR LIBZIP_LIBRARY)
