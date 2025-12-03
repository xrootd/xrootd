
set(XrdClCurl "XrdClCurl-${PLUGIN_VERSION}")

if( FORCE_ENABLED )
  find_package( CURL 7.9.6 REQUIRED )
else()
  find_package( CURL 7.9.6 )
endif()

if( NOT CURL_FOUND )
  return()
endif()

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

# Note the object library's dependencys are listed as `PUBLIC`; this allows
# the subsequent targets (XrdClCurl) to have the correct linkage without repeating
# the dependency.
target_link_libraries( XrdClCurlObj PUBLIC
  CURL::libcurl
  OpenSSL::Crypto
  Threads::Threads
  XrdXml
  XrdCl
  XrdUtils
)

set_target_properties( XrdClCurlObj PROPERTIES POSITION_INDEPENDENT_CODE ON )

add_library( ${XrdClCurl} MODULE "$<TARGET_OBJECTS:XrdClCurlObj>")
target_link_libraries( ${XrdClCurl} XrdClCurlObj)

if(NOT APPLE)
  target_link_options(${XrdClCurl} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols")
endif()

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/configs/xrdcl-curl-plugin.conf ${CMAKE_CURRENT_BINARY_DIR}/configs/xrdcl-curl-plugin.conf)

install(TARGETS ${XrdClCurl} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# We plan to only enable this by default for the XRootD 6.x series
if ( XRootD_VERSION_MAJOR GREATER 5 )
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/configs/xrdcl-curl-plugin.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xrootd/client.plugins.d/
  )
endif()

install(
  FILES
    XrdClCurlConnectionCallout.hh
    XrdClCurlHeaderCallout.hh
    XrdClCurlResponseInfo.hh
    XrdClCurlResponses.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdClCurl
)

