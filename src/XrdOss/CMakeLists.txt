#-----------------------------------------------------------------------------
# XrdOss - Default storage system
#-----------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdOss.cc        XrdOss.hh
    XrdOssAt.cc      XrdOssAt.hh
    XrdOssAio.cc
                     XrdOssError.hh
                     XrdOssDefaultSS.hh
    XrdOssApi.cc     XrdOssApi.hh
    XrdOssCache.cc   XrdOssCache.hh
    XrdOssConfig.cc  XrdOssConfig.hh
    XrdOssCopy.cc    XrdOssCopy.hh
    XrdOssCreate.cc
                     XrdOssOpaque.hh
    XrdOssMio.cc     XrdOssMio.hh
                     XrdOssMioFile.hh
    XrdOssMSS.cc
    XrdOssPath.cc    XrdOssPath.hh
    XrdOssReloc.cc
    XrdOssRename.cc
    XrdOssSpace.cc   XrdOssSpace.hh
    XrdOssStage.cc   XrdOssStage.hh
    XrdOssStat.cc    XrdOssStatInfo.hh
                     XrdOssTrace.hh
    XrdOssUnlink.cc
                     XrdOssWrapper.hh
                     XrdOssVS.hh
)

#-------------------------------------------------------------------------------
# GPFS stat() plugin library
#-------------------------------------------------------------------------------
set(XrdOssSIgpfsT XrdOssSIgpfsT-${PLUGIN_VERSION})
add_library(${XrdOssSIgpfsT} MODULE XrdOssSIgpfsT.cc)
target_link_libraries(${XrdOssSIgpfsT} PRIVATE XrdUtils)

install(TARGETS ${XrdOssSIgpfsT} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
