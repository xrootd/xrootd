

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_POSIX_VERSION   3.0.0 )
set( XRD_POSIX_SOVERSION 3 )
set( XRD_POSIX_PRELOAD_VERSION   2.0.0 )
set( XRD_POSIX_PRELOAD_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdPosix library
#-------------------------------------------------------------------------------
add_library(
  XrdPosix
  SHARED
  XrdPosix/XrdPosixAdmin.cc        XrdPosix/XrdPosixAdmin.hh
  XrdPosix/XrdPosixCache.cc        XrdPosix/XrdPosixCache.hh
  XrdPosix/XrdPosixCallBack.cc     XrdPosix/XrdPosixCallBack.hh
  XrdPosix/XrdPosixConfig.cc       XrdPosix/XrdPosixConfig.hh
  XrdPosix/XrdPosixDir.cc          XrdPosix/XrdPosixDir.hh
  XrdPosix/XrdPosixExtra.cc        XrdPosix/XrdPosixExtra.hh
  XrdPosix/XrdPosixFile.cc         XrdPosix/XrdPosixFile.hh
  XrdPosix/XrdPosixFileRH.cc       XrdPosix/XrdPosixFileRH.hh
  XrdPosix/XrdPosixMap.cc          XrdPosix/XrdPosixMap.hh
  XrdPosix/XrdPosixObject.cc       XrdPosix/XrdPosixObject.hh
                                   XrdPosix/XrdPosixObjGuard.hh
  XrdPosix/XrdPosixPrepIO.cc       XrdPosix/XrdPosixPrepIO.hh
                                   XrdPosix/XrdPosixStats.hh
                                   XrdPosix/XrdPosixTrace.hh
  XrdPosix/XrdPosixXrootd.cc       XrdPosix/XrdPosixXrootd.hh
  XrdPosix/XrdPosixXrootdPath.cc   XrdPosix/XrdPosixXrootdPath.hh
                                   XrdPosix/XrdPosixOsDep.hh    )

target_link_libraries(
  XrdPosix
  PRIVATE
  XrdCl
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT} )

set_target_properties(
  XrdPosix
  PROPERTIES
  VERSION   ${XRD_POSIX_VERSION}
  SOVERSION ${XRD_POSIX_SOVERSION} )

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
  PRIVATE
  XrdPosix
  ${CMAKE_DL_LIBS} )

set_target_properties(
  XrdPosixPreload
  PROPERTIES
  VERSION   ${XRD_POSIX_PRELOAD_VERSION}
  SOVERSION ${XRD_POSIX_PRELOAD_SOVERSION} )

# This is a special library meant to be loaded with LD_PRELOAD.
# It is meant to replace symbols from the system and as such
# must not be compiled with link-time optimizations.

target_compile_options(XrdPosixPreload PRIVATE -fno-lto)

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdPosix XrdPosixPreload
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
