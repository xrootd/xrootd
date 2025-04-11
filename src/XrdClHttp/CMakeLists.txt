if(NOT BUILD_XRDCLHTTP)
  return()
endif()

set(XrdClHttp "XrdClHttp-${PLUGIN_VERSION}")

add_library(${XrdClHttp} MODULE
  XrdClHttpFilePlugIn.cc
  XrdClHttpFileSystemPlugIn.cc
  XrdClHttpPlugInFactory.cc
  XrdClHttpPlugInUtil.cc
  XrdClHttpPosix.cc
)

target_link_libraries(${XrdClHttp} PRIVATE XrdCl XrdUtils Davix::Davix)

install(TARGETS ${XrdClHttp} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
