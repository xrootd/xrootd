
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_POSIX_VERSION   0.0.1 )
set( XRD_POSIX_SOVERSION 0 )
set( XRD_POSIX_PRELOAD_VERSION   0.0.1 )
set( XRD_POSIX_PRELOAD_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdPosix library
#-------------------------------------------------------------------------------
add_library(
  XrdPosix
  SHARED
  XrdPosix/XrdPosixXrootd.cc       XrdPosix/XrdPosixXrootd.hh
  XrdPosix/XrdPosixXrootdPath.cc   XrdPosix/XrdPosixXrootdPath.hh
                                   XrdPosix/XrdPosixCallBack.hh
                                   XrdPosix/XrdPosixOsDep.hh    )

target_link_libraries(
  XrdPosix
  XrdClient
  XrdUtils
  pthread )

set_target_properties(
  XrdPosix
  PROPERTIES
  VERSION   ${XRD_POSIX_VERSION}
  SOVERSION ${XRD_POSIX_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdPosixPreload library
#-------------------------------------------------------------------------------
add_library(
  XrdPosixPreload
  SHARED
  XrdPosix/XrdPosixPreload32.cc
  XrdPosix/XrdPosixPreload.cc
  XrdPosix/XrdPosix.cc           XrdPosix/XrdPosix.hh
  XrdPosix/XrdPosixLinkage.cc    XrdPosix/XrdPosixLinkage.hh
                                 XrdPosix/XrdPosixExtern.hh
                                 XrdPosix/XrdPosixOsDep.hh )

target_link_libraries(
  XrdPosixPreload
  XrdPosix
  dl )

set_target_properties(
  XrdPosixPreload
  PROPERTIES
  VERSION   ${XRD_POSIX_PRELOAD_VERSION}
  SOVERSION ${XRD_POSIX_PRELOAD_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdPosix XrdPosixPreload
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdPosix/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdPosix
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
