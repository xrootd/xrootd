include_directories( ${XROOTD_INCLUDE_DIR} )
include_directories( ${RADOS_INCLUDE_DIR} )
include_directories( ${CMAKE_SOURCE_DIR}/src )


#-------------------------------------------------------------------------------
# XrdCephPosix library version
#-------------------------------------------------------------------------------
set( XRD_CEPH_POSIX_VERSION   0.0.1 )
set( XRD_CEPH_POSIX_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdCephPosix library
#-------------------------------------------------------------------------------
add_library(
  XrdCephPosix
  SHARED
  XrdCeph/XrdCephPosix.cc     XrdCeph/XrdCephPosix.hh )

# needed during the transition between ceph giant and ceph hammer
# for object listing API
set_property(SOURCE XrdCeph/XrdCephPosix.cc
  PROPERTY COMPILE_FLAGS " -Wno-deprecated-declarations")

target_link_libraries(
  XrdCephPosix
  PRIVATE
  ${XROOTD_LIBRARIES}  
  ${RADOS_LIBS} )

set_target_properties(
  XrdCephPosix
  PROPERTIES
  VERSION   ${XRD_CEPH_POSIX_VERSION}
  SOVERSION ${XRD_CEPH_POSIX_SOVERSION} )

#-------------------------------------------------------------------------------
# The XrdCeph module
#-------------------------------------------------------------------------------
set( LIB_XRD_CEPH XrdCeph-${PLUGIN_VERSION} )

add_library(
  ${LIB_XRD_CEPH}
  MODULE
  XrdCeph/XrdCephOss.cc       XrdCeph/XrdCephOss.hh
  XrdCeph/XrdCephOssFile.cc   XrdCeph/XrdCephOssFile.hh
  XrdCeph/XrdCephOssDir.cc    XrdCeph/XrdCephOssDir.hh )

target_link_libraries(
  ${LIB_XRD_CEPH}
  PRIVATE
  ${XROOTD_LIBRARIES}  
  XrdCephPosix )

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
  PRIVATE
  ${XROOTD_LIBRARIES}  
  XrdCephPosix )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_CEPH} ${LIB_XRD_CEPH_XATTR} XrdCephPosix
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
