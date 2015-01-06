
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CEPH_VERSION   1.0.0 )
set( XRD_CEPH_SOVERSION 1 )

set( XRD_CEPH_XATTR_VERSION   1.0.0 )
set( XRD_CEPH_XATTR_SOVERSION 1 )

find_package(ceph REQUIRED)
include_directories( ${RADOS_INCLUDE_DIR} )

#-------------------------------------------------------------------------------
# The XrdCeph lib
#-------------------------------------------------------------------------------
add_library(
  XrdCeph
  SHARED
  XrdCeph/CephOss.cc     XrdCeph/CephOss.hh
  XrdCeph/CephOssFile.cc XrdCeph/CephOssFile.hh
  XrdCeph/CephOssDir.cc  XrdCeph/CephOssDir.hh
  XrdCeph/ceph_posix.cpp XrdCeph/ceph_posix.h )

target_link_libraries(
  XrdCeph
  XrdUtils
  ${RADOS_LIBS} )

set_target_properties(
  XrdCeph
  PROPERTIES
  VERSION   ${XRD_CEPH_VERSION}
  SOVERSION ${XRD_CEPH_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdCephXattr lib
#-------------------------------------------------------------------------------
add_library(
  XrdCephXattr
  SHARED
  XrdCeph/CephXAttr.cc XrdCeph/CephXAttr.hh )

target_link_libraries(
  XrdCeph
  XrdCephXattr
  XrdUtils
  ${RADOS_LIBS} )

set_target_properties(
  XrdCephXattr
  PROPERTIES
  VERSION   ${XRD_CEPH_XATTR_VERSION}
  SOVERSION ${XRD_CEPH_XATTR_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCeph XrdCephXattr
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
