if(NOT BUILD_XRDOSSARC)
  return()
endif()

set(XrdOssArc XrdOssArc-${PLUGIN_VERSION})

add_library(${XrdOssArc}
  MODULE
  XrdOssArc.cc           XrdOssArc.hh
  XrdOssArcConfig.cc     XrdOssArcConfig.hh
  XrdOssArcDataset.cc    XrdOssArcDataset.hh
  XrdOssArcFile.cc       XrdOssArcFile.hh
  XrdOssArcPrep.cc
  XrdOssArcRecompose.cc  XrdOssArcRecompose.hh
  XrdOssArcStage.cc      XrdOssArcStage.hh
                         XrdOssArcTrace.hh
  XrdOssArcZipFile.cc    XrdOssArcZipFile.hh
)

target_link_libraries(${XrdOssArc} PRIVATE XrdUtils zip ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ${XrdOssArc} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
