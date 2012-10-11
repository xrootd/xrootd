# - Find LibEvent (a cross event library)
# This module defines
# LIBEVENTPTHREADS_INCLUDE_DIR, where to find LibEvent headers
# LIBEVENTPTHREADS_LIB, LibEvent libraries
# LIBEVENTPTHREADS_FOUND, If false, do not try to use libevent

find_path(
  LIBEVENTPTHREADS_INCLUDE_DIR
  event2/thread.h
  HINTS
  ${LIBEVENT_DIR}
  $ENV{LIBEVENT_DIR}
  /usr
  /opt
  PATH_SUFFIXES include )

find_library(
  LIBEVENTPTHREADS_LIB
  NAMES event_pthreads
  HINTS
  ${LIBEVENT_DIR}
  $ENV{LIBEVENT_DIR}
  /usr
  /opt
  PATH_SUFFIXES lib )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LibEventPthreads
  DEFAULT_MSG
  LIBEVENTPTHREADS_LIB
  LIBEVENTPTHREADS_INCLUDE_DIR )
