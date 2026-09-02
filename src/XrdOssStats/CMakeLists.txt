set(XrdOssStats XrdOssStats-${PLUGIN_VERSION})

add_library(${XrdOssStats} MODULE
  XrdOssStatsConfig.cc     XrdOssStatsConfig.hh
  XrdOssStatsFile.cc       XrdOssStatsFile.hh
  XrdOssStatsFileSystem.cc XrdOssStatsFileSystem.hh
)

target_link_libraries(${XrdOssStats} PRIVATE XrdServer XrdUtils)

set_target_properties(${XrdOssStats}
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN TRUE
)

install(TARGETS ${XrdOssStats} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
