set(XrdPss XrdPss-${PLUGIN_VERSION})

add_library(${XrdPss} MODULE
  XrdPss.cc         XrdPss.hh
  XrdPssAio.cc
  XrdPssAioCB.cc    XrdPssAioCB.hh
  XrdPssCks.cc      XrdPssCks.hh
  XrdPssConfig.cc
                    XrdPssTrace.hh
  XrdPssUrlInfo.cc  XrdPssUrlInfo.hh
  XrdPssUtils.cc    XrdPssUtils.hh
)

target_link_libraries(${XrdPss} PRIVATE XrdPosix XrdUtils XrdServer)

install(TARGETS ${XrdPss} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
