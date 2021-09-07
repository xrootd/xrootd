#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_OSSCSI  XrdOssCsi-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# The XrdPfc library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_OSSCSI}
  MODULE
  XrdOssCsi/XrdOssCsi.cc                      XrdOssCsi/XrdOssCsi.hh
  XrdOssCsi/XrdOssCsiFile.cc
  XrdOssCsi/XrdOssCsiFileAio.cc               XrdOssCsi/XrdOssCsiFileAio.hh
  XrdOssCsi/XrdOssCsiPages.cc                 XrdOssCsi/XrdOssCsiPages.hh
  XrdOssCsi/XrdOssCsiPagesUnaligned.cc
  XrdOssCsi/XrdOssCsiTagstoreFile.cc          XrdOssCsi/XrdOssCsiTagstoreFile.hh
  XrdOssCsi/XrdOssCsiRanges.cc                XrdOssCsi/XrdOssCsiRanges.hh
  XrdOssCsi/XrdOssCsiConfig.cc                XrdOssCsi/XrdOssCsiConfig.hh
  XrdOssCsi/XrdOssCsiCrcUtils.cc              XrdOssCsi/XrdOssCsiCrcUtils.hh
                                              XrdOssCsi/XrdOssCsiTagstore.hh
                                              XrdOssCsi/XrdOssHandler.hh
                                              XrdOssCsi/XrdOssCsiTrace.hh
  )

target_link_libraries(
  ${LIB_XRD_OSSCSI}
  XrdUtils
  XrdServer
  ${CMAKE_THREAD_LIBS_INIT} )

set_target_properties(
  ${LIB_XRD_OSSCSI}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_OSSCSI}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
