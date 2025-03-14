set(XrdOssStats XrdOssStats-${PLUGIN_VERSION})

add_library(${XrdOssStats} MODULE
  XrdOssStatsConfig.cc     XrdOssStatsConfig.hh
  XrdOssStatsFile.cc       XrdOssStatsFile.hh
  XrdOssStatsFileSystem.cc XrdOssStatsFileSystem.hh
)

target_link_libraries(${XrdOssStats} PRIVATE XrdServer XrdUtils)

if(NOT APPLE)
  target_link_options(${XrdOssStats} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export-lib-symbols")
endif()

install(TARGETS ${XrdOssStats} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
