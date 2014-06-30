# - Find ceph
#
# RADOS_INCLUDE_DIR        - location of the ceph-devel header files for rados
# RADOS_LIBS               - list of rados libraries, with full path
#

# Be silent if CEPH_INCLUDE_DIR is already cached
if (RADOS_INCLUDE_DIR)
  set(RADOS_FIND_QUIETLY TRUE)
endif (RADOS_INCLUDE_DIR)

find_path (RADOS_INCLUDE_DIR NAMES radosstriper/libradosstriper.hpp
  PATH_SUFFIXES include/radosstriper
)

find_library (RADOSSTRIPER_LIB radosstriper)
find_library (RADOS_LIB rados)
set(RADOS_LIBS ${RADOS_LIB} ${RADOSSTRIPER_LIB})

message (STATUS "RADOS_INCLUDE_DIR        = ${RADOS_INCLUDE_DIR}")
message (STATUS "RADOS_LIBS               = ${RADOS_LIBS}")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ceph DEFAULT_MSG 
  RADOS_INCLUDE_DIR
  RADOS_LIBS)

