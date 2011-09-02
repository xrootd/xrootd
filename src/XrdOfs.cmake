
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_OFS_VERSION   0.0.1 )
set( XRD_OFS_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdClient lib
#-------------------------------------------------------------------------------
add_library(
  XrdOfs
  SHARED
  XrdOfs/XrdOfs.cc              XrdOfs/XrdOfs.hh
                                XrdOfs/XrdOfsSecurity.hh
                                XrdOfs/XrdOfsTrace.hh
  XrdOfs/XrdOfsFS.cc
  XrdOfs/XrdOfsConfig.cc        XrdOfs/XrdOfsConfig.hh
  XrdOfs/XrdOfsEvr.cc           XrdOfs/XrdOfsEvr.hh
  XrdOfs/XrdOfsEvs.cc           XrdOfs/XrdOfsEvs.hh
  XrdOfs/XrdOfsHandle.cc        XrdOfs/XrdOfsHandle.hh
  XrdOfs/XrdOfsPoscq.cc         XrdOfs/XrdOfsPoscq.hh
  XrdOfs/XrdOfsStats.cc         XrdOfs/XrdOfsStats.hh )

target_link_libraries(
  XrdOfs
  XrdUtils
  XrdServer )

set_target_properties(
  XrdOfs
  PROPERTIES
  VERSION   ${XRD_OFS_VERSION}
  SOVERSION ${XRD_OFS_SOVERSION} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdOfs
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdOfs/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdOfs
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
