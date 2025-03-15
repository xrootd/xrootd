target_sources(XrdUtils
  PRIVATE
    XrdCksAssist.cc      XrdCksAssist.hh
    XrdCksCalccrc32.cc   XrdCksCalccrc32.hh
    XrdCksCalccrc32C.cc  XrdCksCalccrc32C.hh
    XrdCksCalcmd5.cc     XrdCksCalcmd5.hh
    XrdCksConfig.cc      XrdCksConfig.hh
    XrdCksLoader.cc      XrdCksLoader.hh
    XrdCksManager.cc     XrdCksManager.hh
    XrdCksManOss.cc      XrdCksManOss.hh
                         XrdCksCalcadler32.hh
                         XrdCksCalc.hh
                         XrdCksData.hh
                         XrdCks.hh
                         XrdCksXAttr.hh
)

#-------------------------------------------------------------------------------
# libz compatible CRC32 plugin
#-------------------------------------------------------------------------------
set(XrdClsCalczcrc32 XrdCksCalczcrc32-${PLUGIN_VERSION})

add_library(${XrdClsCalczcrc32} MODULE XrdCksCalczcrc32.cc)
target_link_libraries(${XrdClsCalczcrc32} PRIVATE XrdUtils ZLIB::ZLIB)

install(TARGETS ${XrdClsCalczcrc32} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
