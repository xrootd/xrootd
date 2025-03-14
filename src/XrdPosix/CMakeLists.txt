

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
  XrdPosixAdmin.cc        XrdPosixAdmin.hh
  XrdPosixCache.cc        XrdPosixCache.hh
  XrdPosixCallBack.cc     XrdPosixCallBack.hh
  XrdPosixConfig.cc       XrdPosixConfig.hh
  XrdPosixDir.cc          XrdPosixDir.hh
  XrdPosixExtra.cc        XrdPosixExtra.hh
  XrdPosixFile.cc         XrdPosixFile.hh
  XrdPosixFileRH.cc       XrdPosixFileRH.hh
  XrdPosixMap.cc          XrdPosixMap.hh
  XrdPosixObject.cc       XrdPosixObject.hh
                          XrdPosixObjGuard.hh
  XrdPosixPrepIO.cc       XrdPosixPrepIO.hh
                          XrdPosixStats.hh
                          XrdPosixTrace.hh
  XrdPosixXrootd.cc       XrdPosixXrootd.hh
  XrdPosixXrootdPath.cc   XrdPosixXrootdPath.hh
                          XrdPosixOsDep.hh    )

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
  XrdPosixPreload32.cc
  XrdPosixPreload.cc
  XrdPosix.cc           XrdPosix.hh
  XrdPosixLinkage.cc    XrdPosixLinkage.hh
                        XrdPosixExtern.hh
                        XrdPosixOsDep.hh )

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

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag(-fno-lto HAS_NOLTO)

if(HAS_NOLTO)
  target_compile_options(XrdPosixPreload PRIVATE -fno-lto)
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdPosix XrdPosixPreload
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
