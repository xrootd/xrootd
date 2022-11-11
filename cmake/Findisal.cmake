#.rst:
# Findisal
# ---------
#
# Find Intelligent Storage Acceleration Library.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   ISAL_FOUND          - True if isa-l is found.
#   ISAL_INCLUDE_DIRS   - Where to find isa-l.h
#   ISAL_LIBRARIES      - Where to find libisal.so
#
# ::
#
#   ISAL_VERSION        - The version of ISAL found (x.y.z)
#   ISAL_VERSION_MAJOR  - The major version of isa-l
#   ISAL_VERSION_MINOR  - The minor version of isa-l
#   ISAL_VERSION_PATCH  - The patch version of isa-l

foreach(var ISAL_FOUND ISAL_INCLUDE_DIR ISAL_ISAL_LIBRARY ISAL_LIBRARIES)
  unset(${var} CACHE)
endforeach()

find_path(ISAL_INCLUDE_DIR NAME isa-l.h PATH_SUFFIXES include)

if(NOT ISAL_LIBRARY)
  find_library(ISAL_LIBRARY NAMES isal PATH_SUFFIXES lib)
endif()

mark_as_advanced(ISAL_INCLUDE_DIR)

if(ISAL_INCLUDE_DIR AND EXISTS "${ISAL_INCLUDE_DIR}/isa-l.h")
  file(STRINGS "${ISAL_INCLUDE_DIR}/isa-l.h" ISAL_H REGEX "^#define ISAL_[A-Z_]+[ ]+[0-9]+.*$")
  string(REGEX REPLACE ".+ISAL_MAJOR_VERSION[ ]+([0-9]+).*$" "\\1" ISAL_VERSION_MAJOR "${ISAL_H}")
  string(REGEX REPLACE ".+ISAL_MINOR_VERSION[ ]+([0-9]+).*$" "\\1" ISAL_VERSION_MINOR "${ISAL_H}")
  string(REGEX REPLACE ".+ISAL_PATCH_VERSION[ ]+([0-9]+).*$" "\\1" ISAL_VERSION_PATCH "${ISAL_H}")
  set(ISAL_VERSION "${ISAL_VERSION_MAJOR}.${ISAL_VERSION_MINOR}.${ISAL_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(isal
  REQUIRED_VARS ISAL_LIBRARY ISAL_INCLUDE_DIR VERSION_VAR ISAL_VERSION)

if(ISAL_FOUND)
  set(ISAL_INCLUDE_DIRS "${ISAL_INCLUDE_DIR}")

  if(NOT ISAL_LIBRARIES)
    set(ISAL_LIBRARIES ${ISAL_LIBRARY})
  endif()
endif()
