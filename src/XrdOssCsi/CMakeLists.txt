set(XrdOssCsi XrdOssCsi-${PLUGIN_VERSION})

add_library(${XrdOssCsi} MODULE
  XrdOssCsi.cc                XrdOssCsi.hh
  XrdOssCsiFile.cc
  XrdOssCsiFileAio.cc         XrdOssCsiFileAio.hh
  XrdOssCsiPages.cc           XrdOssCsiPages.hh
  XrdOssCsiPagesUnaligned.cc
  XrdOssCsiTagstoreFile.cc    XrdOssCsiTagstoreFile.hh
  XrdOssCsiRanges.cc          XrdOssCsiRanges.hh
  XrdOssCsiConfig.cc          XrdOssCsiConfig.hh
  XrdOssCsiCrcUtils.cc        XrdOssCsiCrcUtils.hh
                              XrdOssCsiTagstore.hh
                              XrdOssHandler.hh
                              XrdOssCsiTrace.hh
)

target_link_libraries(${XrdOssCsi} PRIVATE XrdUtils XrdServer)

install(TARGETS ${XrdOssCsi} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
