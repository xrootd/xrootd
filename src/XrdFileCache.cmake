include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_FILE_CACHE_VERSION   1.0.0 )
set( XRD_FILE_CACHE_SOVERSION 1 )

#-------------------------------------------------------------------------------
# The XrdFileCache library
#-------------------------------------------------------------------------------
add_library(
  XrdFileCache
  SHARED
  XrdFileCache/Cache.cc              XrdFileCache/Cache.hh
  XrdFileCache/Factory.cc            XrdFileCache/Factory.hh
  XrdFileCache/CacheStats.cc         XrdFileCache/CacheStats.hh
  XrdFileCache/CacheFileInfo.cc      XrdFileCache/CacheFileInfo.hh
  XrdFileCache/IO.cc                 XrdFileCache/IO.hh
  XrdFileCache/IOBlocks.cc           XrdFileCache/IOBlocks.hh
  XrdFileCache/Context.cc            XrdFileCache/Context.hh )

target_link_libraries(
  XrdFileCache
  XrdPosix
  XrdCl
  XrdUtils
  pthread )

set_target_properties(
  XrdFileCache
  PROPERTIES
  VERSION   ${XRD_FILE_CACHE_VERSION}
  SOVERSION ${XRD_FILE_CACHE_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdFileCache
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
