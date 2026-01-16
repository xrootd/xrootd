if(FORCE_ENABLED)
  find_package(CURL REQUIRED)
else()
  find_package(CURL)
endif()

if(NOT CURL_FOUND)
  return()
endif()

add_library(XrdClHttpObj OBJECT
  XrdClHttpFactory.cc        XrdClHttpFactory.hh
  XrdClHttpFile.cc           XrdClHttpFile.hh
  XrdClHttpFilesystem.cc     XrdClHttpFilesystem.hh
  XrdClHttpOpChecksum.cc
  XrdClHttpOpCopy.cc
  XrdClHttpOpDelete.cc
  XrdClHttpOpListdir.cc
  XrdClHttpOpMkcol.cc
  XrdClHttpOpOpen.cc
  XrdClHttpOpOptions.cc
  XrdClHttpOpPut.cc
  XrdClHttpOpQuery.cc
  XrdClHttpOpRead.cc
  XrdClHttpOpReadV.cc
  XrdClHttpOps.cc            XrdClHttpOps.hh
  XrdClHttpOpStat.cc
  XrdClHttpOptionsCache.cc   XrdClHttpOptionsCache.hh
  XrdClHttpParseTimeout.cc   XrdClHttpParseTimeout.hh
  XrdClHttpUtil.cc           XrdClHttpUtil.hh
  XrdClHttpWorker.hh
)

target_link_libraries(XrdClHttpObj
  PRIVATE
    XrdCl
    XrdUtils
    XrdXml
    CURL::libcurl
    OpenSSL::Crypto
    Threads::Threads
)

set_target_properties(XrdClHttpObj PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(XrdClHttp "XrdClHttp-${PLUGIN_VERSION}")

add_library(${XrdClHttp} MODULE)
target_link_libraries(${XrdClHttp} PRIVATE XrdClHttpObj)

if(NOT APPLE)
  target_link_options(${XrdClHttp} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols")
endif()

install(TARGETS ${XrdClHttp} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
  FILES
    XrdClHttpConnectionCallout.hh
    XrdClHttpHeaderCallout.hh
    XrdClHttpResponseInfo.hh
    XrdClHttpResponses.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdClHttp
)
