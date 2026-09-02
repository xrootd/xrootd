unset(BUILD_TPC CACHE)

if(NOT ENABLE_HTTP)
  return()
endif()

if(FORCE_ENABLED)
  find_package(CURL REQUIRED)
else()
  find_package(CURL)
endif()

if(CURL_FOUND)
  set(BUILD_TPC TRUE CACHE INTERNAL "")
else()
  return()
endif()

set(XrdHttpTPC XrdHttpTPC-${PLUGIN_VERSION})

add_library(${XrdHttpTPC} MODULE
  XrdHttpTpcConfigure.cc
  XrdHttpTpcMultistream.cc
  XrdHttpTpcPMarkManager.cc  XrdHttpTpcPMarkManager.hh
  XrdHttpTpcState.cc         XrdHttpTpcState.hh
  XrdHttpTpcStream.cc        XrdHttpTpcStream.hh
  XrdHttpTpcTPC.cc           XrdHttpTpcTPC.hh
  XrdHttpTpcUtils.cc         XrdHttpTpcUtils.hh
)

target_link_libraries(${XrdHttpTPC}
  PRIVATE
    XrdServer
    XrdUtils
    XrdHttpUtils
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

set_target_properties(${XrdHttpTPC}
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN TRUE
)

install(TARGETS ${XrdHttpTPC} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
