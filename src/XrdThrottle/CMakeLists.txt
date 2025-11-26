set(XrdThrottle XrdThrottle-${PLUGIN_VERSION})

add_library(${XrdThrottle} MODULE
  ${PROJECT_SOURCE_DIR}/src/XrdOfs/XrdOfsFS.cc
  XrdThrottleConfig.cc XrdThrottleConfig.hh
  XrdThrottle.hh           XrdThrottleTrace.hh
  XrdThrottleFileSystem.cc
  XrdThrottleFileSystemConfig.cc
  XrdThrottleFile.cc
  XrdOssThrottleFile.cc
  XrdThrottleManager.cc    XrdThrottleManager.hh
)

target_link_libraries(${XrdThrottle} PRIVATE XrdServer XrdUtils)

target_include_directories(${XrdThrottle}
  PRIVATE
    ../XrdSciTokens/vendor/inih
)

install(TARGETS ${XrdThrottle} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
