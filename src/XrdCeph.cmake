include( XRootDCommon )
include_directories( ${RADOS_INCLUDE_DIR} )

#-------------------------------------------------------------------------------
# The XrdCeph module
#-------------------------------------------------------------------------------
set( LIB_XRD_CEPH XrdCeph-${PLUGIN_VERSION} )

add_library(
  ${LIB_XRD_CEPH}
  MODULE
  XrdCeph/XrdCephOss.cc       XrdCeph/XrdCephOss.hh
  XrdCeph/XrdCephOssFile.cc   XrdCeph/XrdCephOssFile.hh
  XrdCeph/XrdCephOssDir.cc    XrdCeph/XrdCephOssDir.hh
  XrdCeph/XrdCephPosix.cc     XrdCeph/XrdCephPosix.hh )

target_link_libraries(
  ${LIB_XRD_CEPH}
  XrdUtils
  ${RADOS_LIBS} )

set_target_properties(
  ${LIB_XRD_CEPH}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdCephXattr module
#-------------------------------------------------------------------------------
set( LIB_XRD_CEPH_XATTR XrdCephXattr-${PLUGIN_VERSION} )

add_library(
  ${LIB_XRD_CEPH_XATTR}
  MODULE
  XrdCeph/XrdCephXAttr.cc   XrdCeph/XrdCephXAttr.hh )

target_link_libraries(
  ${LIB_XRD_CEPH_XATTR}
  XrdUtils
  ${RADOS_LIBS} )

set_target_properties(
  ${LIB_XRD_CEPH_XATTR}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_CEPH} ${LIB_XRD_CEPH_XATTR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
