# Try to find systemd
# Once done, this will define
#
# SYSTEMD_FOUND - system has systemd
# SYSTEMD_INCLUDE_DIRS - the systemd include directories
# SYSTEMD_LIBRARIES - systemd libraries directories

find_path( SYSTEMD_INCLUDE_DIR systemd/sd-daemon.h
  HINTS
  ${SYSTEMD_DIR}
  $ENV{SYSTEMD_DIR}
  /usr
  /opt
  PATH_SUFFIXES include
)

find_library( SYSTEMD_LIBRARY systemd
  HINTS
  ${SYSTEMD_DIR}
  $ENV{SYSTEMD_DIR}
  /usr
  /opt
  PATH_SUFFIXES lib
)

set(SYSTEMD_INCLUDE_DIRS ${SYSTEMD_INCLUDE_DIR})
set(SYSTEMD_LIBRARIES ${SYSTEMD_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(systemd DEFAULT_MSG SYSTEMD_INCLUDE_DIRS SYSTEMD_LIBRARIES)
