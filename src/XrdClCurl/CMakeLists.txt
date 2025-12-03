if(FORCE_ENABLED)
  find_package(CURL REQUIRED)
else()
  find_package(CURL)
endif()

if(NOT CURL_FOUND)
  return()
endif()

add_library(XrdClCurlObj OBJECT
  XrdClCurlFactory.cc        XrdClCurlFactory.hh
  XrdClCurlFile.cc           XrdClCurlFile.hh
  XrdClCurlFilesystem.cc     XrdClCurlFilesystem.hh
  XrdClCurlOpChecksum.cc
  XrdClCurlOpCopy.cc
  XrdClCurlOpDelete.cc
  XrdClCurlOpListdir.cc
  XrdClCurlOpMkcol.cc
  XrdClCurlOpOpen.cc
  XrdClCurlOpOptions.cc
  XrdClCurlOpPut.cc
  XrdClCurlOpQuery.cc
  XrdClCurlOpRead.cc
  XrdClCurlOpReadV.cc
  XrdClCurlOps.cc            XrdClCurlOps.hh
  XrdClCurlOpStat.cc
  XrdClCurlOptionsCache.cc   XrdClCurlOptionsCache.hh
  XrdClCurlParseTimeout.cc   XrdClCurlParseTimeout.hh
  XrdClCurlUtil.cc           XrdClCurlUtil.hh
  XrdClCurlWorker.hh
)

target_link_libraries(XrdClCurlObj
  PRIVATE
    XrdCl
    XrdUtils
    XrdXml
    CURL::libcurl
    OpenSSL::Crypto
    Threads::Threads
)

set_target_properties(XrdClCurlObj PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(XrdClCurl "XrdClCurl-${PLUGIN_VERSION}")

add_library(${XrdClCurl} MODULE)
target_link_libraries(${XrdClCurl} PRIVATE XrdClCurlObj)

if(NOT APPLE)
  target_link_options(${XrdClCurl} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols")
endif()

install(TARGETS ${XrdClCurl} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
  FILES
    XrdClCurlConnectionCallout.hh
    XrdClCurlHeaderCallout.hh
    XrdClCurlResponseInfo.hh
    XrdClCurlResponses.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdClCurl
)
