
set(XrdClS3 "XrdClS3-${PLUGIN_VERSION}")

if( FORCE_ENABLED )
  find_package( CURL 7.9.6 REQUIRED )
else()
  find_package( CURL 7.9.6 )
endif()

if( NOT CURL_FOUND )
  return()
endif()

add_library( XrdClS3Obj OBJECT
  XrdClS3DownloadHandler.cc XrdClS3DownloadHandler.hh
  XrdClS3Factory.cc         XrdClS3Factory.hh
  XrdClS3File.cc            XrdClS3File.hh
  XrdClS3Filesystem.cc      XrdClS3Filesystem.hh
)

target_link_libraries( XrdClS3Obj PUBLIC
  CURL::libcurl
  OpenSSL::Crypto
  Threads::Threads
  XrdXml
  XrdCl
  XrdUtils
)

if(TINYXML_FOUND)
  target_include_directories( XrdClS3Obj PRIVATE ${TINYXML_INCLUDE_DIR} )
else()
  target_include_directories( XrdClS3Obj PRIVATE ../XrdXml/tinyxml )
endif()

set_target_properties( XrdClS3Obj PROPERTIES POSITION_INDEPENDENT_CODE ON )

if (ENABLE_TESTS)
  add_library(XrdClS3Testing SHARED "$<TARGET_OBJECTS:XrdClS3Obj>")
  target_link_libraries(XrdClS3Testing XrdClS3Obj)
  target_include_directories(XrdClS3Testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

add_library( ${XrdClS3} MODULE "$<TARGET_OBJECTS:XrdClS3Obj>")
target_link_libraries( ${XrdClS3} XrdClS3Obj)

if ( NOT APPLE )
  set_target_properties( ${XrdClS3} PROPERTIES
    LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols"
  )
endif()

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/configs/xrdcl-s3-plugin.conf ${CMAKE_CURRENT_BINARY_DIR}/configs/xrdcl-s3-plugin.conf)

install(TARGETS ${XrdClS3} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/configs/xrdcl-s3-plugin.conf
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xrootd/client.plugins.d/
)
