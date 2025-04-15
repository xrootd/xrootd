set(XrdThrottle XrdThrottle-${PLUGIN_VERSION})

add_library(${XrdThrottle} MODULE
  ${PROJECT_SOURCE_DIR}/src/XrdOfs/XrdOfsFS.cc
  XrdThrottle.hh           XrdThrottleTrace.hh
  XrdThrottleFileSystem.cc
  XrdThrottleFileSystemConfig.cc
  XrdThrottleFile.cc
  XrdThrottleManager.cc    XrdThrottleManager.hh
)

target_link_libraries(${XrdThrottle} PRIVATE XrdServer XrdUtils)

install(TARGETS ${XrdThrottle} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
