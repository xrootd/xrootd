include_directories(${Davix_INCLUDE_DIRS}/davix)

set(libXrdClHttp_sources
  XrdClHttpPlugInFactory.cc
  XrdClHttpPlugInUtil.cc
  XrdClHttpFilePlugIn.cc
  XrdClHttpFileSystemPlugIn.cc
  XrdClHttpPosix.cc)

set(PLUGIN_NAME "XrdClHttp-${PLUGIN_VERSION}")

add_library(${PLUGIN_NAME} MODULE ${libXrdClHttp_sources})

target_link_libraries(${PLUGIN_NAME} ${Davix_LIBRARIES} XrdCl XrdUtils)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
