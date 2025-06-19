if(NOT BUILD_XRDOSSARC)
  return()
endif()

set(XrdOssArc XrdOssArc-${PLUGIN_VERSION})

add_library(${XrdOssArc}
  MODULE
  XrdOssArc.cc           XrdOssArc.hh
  XrdOssArcBackup.cc     XrdOssArcBackup.hh
  XrdOssArcCompose.cc    XrdOssArcCompose.hh
  XrdOssArcConfig.cc     XrdOssArcConfig.hh
  XrdOssArcDir.cc        XrdOssArcDir.hh
  XrdOssArcFile.cc       XrdOssArcFile.hh
  XrdOssArcFSMon.cc      XrdOssArcFSMon.hh
  XrdOssArcStage.cc      XrdOssArcStage.hh
  XrdOssArcStopMon.cc    XrdOssArcStopMon.hh
                         XrdOssArcTrace.hh
  XrdOssArcZipFile.cc    XrdOssArcZipFile.hh
)

target_link_libraries(${XrdOssArc}
  PRIVATE
    XrdUtils
    XrdServer
    libzip::zip
    ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS ${XrdOssArc} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
