# - Find ceph
#
# RADOS_INCLUDE_DIR        - location of the ceph-devel header files for rados
# RADOS_LIBS               - list of rados libraries, with full path
# RADOS_FOUND

find_path(
  RADOS_INCLUDE_DIR
  radosstriper/libradosstriper.hpp
  HINTS
  ${CEPH_DIR}
  $ENV{CEPH_DIR}
  /usr
  /opt
  PATH_SUFFIXES include
)

find_library(
  RADOSSTRIPER_LIB
  NAMES radosstriper
  HINTS
  ${CEPH_DIR}
  $ENV{CEPH_DIR}
  /usr
  /opt
  PATH_SUFFIXES lib
)

find_library(
  RADOS_LIB
  NAMES rados
  HINTS
  ${CEPH_DIR}
  $ENV{CEPH_DIR}
  /usr
  /opt
  PATH_SUFFIXES lib
)

set(RADOS_LIBS ${RADOS_LIB} ${RADOSSTRIPER_LIB})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ceph DEFAULT_MSG RADOS_INCLUDE_DIR RADOS_LIBS)
