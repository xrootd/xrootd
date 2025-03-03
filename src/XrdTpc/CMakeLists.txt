unset(BUILD_TPC CACHE)

if(NOT ENABLE_HTTP)
  return()
endif()

if(FORCE_ENABLED)
  find_package(CURL 7.9.6 REQUIRED)
else()
  find_package(CURL 7.9.6)
endif()

if(CURL_FOUND)
  set(BUILD_TPC TRUE CACHE INTERNAL "")
else()
  return()
endif()

set(XrdHttpTPC XrdHttpTPC-${PLUGIN_VERSION})

add_library(${XrdHttpTPC} MODULE
  XrdTpcConfigure.cc
  XrdTpcMultistream.cc
  XrdTpcState.cc         XrdTpcState.hh
  XrdTpcStream.cc        XrdTpcStream.hh
  XrdTpcTPC.cc           XrdTpcTPC.hh
  XrdTpcPMarkManager.cc  XrdTpcPMarkManager.hh
  XrdTpcUtils.cc         XrdTpcUtils.hh)

target_link_libraries(${XrdHttpTPC}
  PRIVATE
    XrdServer
    XrdUtils
    XrdHttpUtils
    CURL::libcurl
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

if(NOT APPLE)
  target_link_options(${XrdHttpTPC} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export-lib-symbols")
endif()

install(TARGETS ${XrdHttpTPC} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
