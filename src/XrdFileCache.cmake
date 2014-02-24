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
  XrdFileCache/XrdFileCache.cc              XrdFileCache/XrdFileCache.hh
  XrdFileCache/XrdFileCacheFactory.cc       XrdFileCache/XrdFileCacheFactory.hh
  XrdFileCache/XrdFileCachePrefetch.cc      XrdFileCache/XrdFileCachePrefetch.hh
  XrdFileCache/XrdFileCacheStats.hh
  XrdFileCache/XrdFileCacheInfo.cc          XrdFileCache/XrdFileCacheInfo.hh
  XrdFileCache/XrdFileCacheIOEntireFile.cc  XrdFileCache/XrdFileCacheIOEntireFile.hh
  XrdFileCache/XrdFileCacheIOFileBlock.cc   XrdFileCache/XrdFileCacheIOFileBlock.hh
  XrdFileCache/XrdFileCacheDecision.hh)

target_link_libraries(
  XrdFileCache
  XrdPosix
  XrdCl
  XrdUtils
  XrdServer
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
