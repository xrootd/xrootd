
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
SET( XRD_FFS_VERSION   2.0.0 )
SET( XRD_FFS_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdFfs library
#-------------------------------------------------------------------------------
add_library(
  XrdFfs
  SHARED
  XrdFfs/XrdFfsDent.cc        XrdFfs/XrdFfsDent.hh
  XrdFfs/XrdFfsFsinfo.cc      XrdFfs/XrdFfsFsinfo.hh
  XrdFfs/XrdFfsMisc.cc        XrdFfs/XrdFfsMisc.hh
  XrdFfs/XrdFfsPosix.cc       XrdFfs/XrdFfsPosix.hh
  XrdFfs/XrdFfsQueue.cc       XrdFfs/XrdFfsQueue.hh
  XrdFfs/XrdFfsWcache.cc      XrdFfs/XrdFfsWcache.hh )

target_link_libraries(
  XrdFfs
  XrdCl
  XrdPosix
  XrdSecsss
  XrdUtils
  pthread )

set_target_properties(
  XrdFfs
  PROPERTIES
  VERSION   ${XRD_FFS_VERSION}
  SOVERSION ${XRD_FFS_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# xrootdfs
#-------------------------------------------------------------------------------
if( BUILD_FUSE )
  add_executable(
    xrootdfs
    XrdFfs/XrdFfsXrootdfs.cc )

  target_link_libraries(
    xrootdfs
    XrdFfs
    XrdPosix
    pthread
    ${FUSE_LIBRARIES} )
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdFfs
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

if( BUILD_FUSE )
  install(
    TARGETS xrootdfs
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

  install(
    FILES
    ${PROJECT_SOURCE_DIR}/docs/man/xrootdfs.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )
endif()
