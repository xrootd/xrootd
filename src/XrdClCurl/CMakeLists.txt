
set(XrdClCurl "XrdClCurl-${PLUGIN_VERSION}")

add_library( XrdClCurlObj OBJECT
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

target_link_libraries( XrdClCurlObj PUBLIC
  CURL::libcurl
  OpenSSL::Crypto
  Threads::Threads
  XrdXml
  XrdCl
  XrdUtils
  XrdServer
)
if ( TINYXML_FOUND )
  target_include_directories( XrdClCurlObj PUBLIC ${TINYXML_INCLUDE_DIR} )
else()
  target_include_directories( XrdClCurlObj PUBLIC ../XrdXml/tinyxml )
endif()

set_target_properties( XrdClCurlObj PROPERTIES POSITION_INDEPENDENT_CODE ON )

if (ENABLE_TESTS)
  add_library(XrdClCurlTesting SHARED "$<TARGET_OBJECTS:XrdClCurlObj>")
  target_link_libraries(XrdClCurlTesting XrdClCurlObj)
  target_include_directories(XrdClCurlTesting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

add_library( ${XrdClCurl} MODULE "$<TARGET_OBJECTS:XrdClCurlObj>")
target_link_libraries( ${XrdClCurl} XrdClCurlObj)

if ( NOT APPLE )
  set_target_properties( ${XrdClCurl} PROPERTIES
    LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols"
  )
endif()

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/configs/xrdcl-curl-plugin.conf ${CMAKE_CURRENT_BINARY_DIR}/configs/xrdcl-curl-plugin.conf)

install(TARGETS ${XrdClCurl} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/configs/xrdcl-curl-plugin.conf
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xrootd/client.plugins.d/
)

install(
  FILES
    XrdClCurlConnectionCallout.hh
    XrdClCurlHeaderCallout.hh
    XrdClCurlResponseInfo.hh
    XrdClCurlResponses.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdClCurl
)

