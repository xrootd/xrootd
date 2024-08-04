

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
SET( XRD_FFS_VERSION   3.0.0 )
SET( XRD_FFS_SOVERSION 3 )

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
  PRIVATE
  XrdCl
  XrdPosix
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT} )

set_target_properties(
  XrdFfs
  PROPERTIES
  VERSION   ${XRD_FFS_VERSION}
  SOVERSION ${XRD_FFS_SOVERSION} )

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
    ${CMAKE_THREAD_LIBS_INIT}
    ${FUSE_LIBRARIES} )

target_include_directories(xrootdfs PRIVATE ${FUSE_INCLUDE_DIR})
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
endif()
