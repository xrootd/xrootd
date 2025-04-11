set(XrdOssCsi XrdOssCsi-${PLUGIN_VERSION})

add_library(${XrdOssCsi} MODULE
  XrdOssCsi.cc                XrdOssCsi.hh
  XrdOssCsiConfig.cc          XrdOssCsiConfig.hh
  XrdOssCsiCrcUtils.cc        XrdOssCsiCrcUtils.hh
  XrdOssCsiFile.cc
  XrdOssCsiFileAio.cc         XrdOssCsiFileAio.hh
  XrdOssCsiPages.cc           XrdOssCsiPages.hh
  XrdOssCsiPagesUnaligned.cc
  XrdOssCsiRanges.cc          XrdOssCsiRanges.hh
  XrdOssCsiTagstore.hh
  XrdOssCsiTagstoreFile.cc    XrdOssCsiTagstoreFile.hh
                              XrdOssCsiTrace.hh
                              XrdOssHandler.hh
)

target_link_libraries(${XrdOssCsi} PRIVATE XrdUtils XrdServer)

install(TARGETS ${XrdOssCsi} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
