#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_OSSARC  XrdOssArc-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# The XrdOssArc library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_OSSARC}
  MODULE
  XrdOssArc/XrdOssArc.cc                      XrdOssArc/XrdOssArc.hh
  XrdOssArc/XrdOssArcConfig.cc                XrdOssArc/XrdOssArcConfig.hh
  XrdOssArc/XrdOssArcDataset.cc               XrdOssArc/XrdOssArcDataset.hh
  XrdOssArc/XrdOssArcFile.cc                  XrdOssArc/XrdOssArcFile.hh
  XrdOssArc/XrdOssArcPrep.cc
  XrdOssArc/XrdOssArcRecompose.cc             XrdOssArc/XrdOssArcRecompose.hh
  XrdOssArc/XrdOssArcStage.cc                 XrdOssArc/XrdOssArcStage.hh
                                              XrdOssArc/XrdOssArcTrace.hh
  XrdOssArc/XrdOssArcZipFile.cc               XrdOssArc/XrdOssArcZipFile.hh
  )

target_link_libraries(
  ${LIB_XRD_OSSARC}
  PRIVATE
  XrdUtils
  zip
  ${CMAKE_THREAD_LIBS_INIT} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_OSSARC}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
