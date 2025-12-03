
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

set_target_properties( XrdClS3Obj PROPERTIES POSITION_INDEPENDENT_CODE ON )

if (ENABLE_TESTS)
  add_library(XrdClS3Testing SHARED "$<TARGET_OBJECTS:XrdClS3Obj>")
  target_link_libraries(XrdClS3Testing XrdClS3Obj)
  target_include_directories(XrdClS3Testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

add_library( ${XrdClS3} MODULE "$<TARGET_OBJECTS:XrdClS3Obj>")
target_link_libraries( ${XrdClS3} XrdClS3Obj)

if(NOT APPLE)
  target_link_options(${XrdClS3} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols")
endif()

install(TARGETS ${XrdClS3} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
