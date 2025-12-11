if(FORCE_ENABLED)
  find_package(CURL REQUIRED)
else()
  find_package(CURL)
endif()

if(NOT CURL_FOUND)
  return()
endif()

add_library(XrdClS3Obj OBJECT
  XrdClS3DownloadHandler.cc XrdClS3DownloadHandler.hh
  XrdClS3Factory.cc         XrdClS3Factory.hh
  XrdClS3File.cc            XrdClS3File.hh
  XrdClS3Filesystem.cc      XrdClS3Filesystem.hh
)

target_link_libraries(XrdClS3Obj
  PRIVATE
    XrdCl
    XrdUtils
    XrdXml
    CURL::libcurl
    OpenSSL::Crypto
    Threads::Threads
)

set_target_properties(XrdClS3Obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(XrdClS3 "XrdClS3-${PLUGIN_VERSION}")

add_library(${XrdClS3} MODULE)
target_link_libraries(${XrdClS3} PRIVATE XrdClS3Obj)

if(NOT APPLE)
  target_link_options(${XrdClS3} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols")
endif()

install(TARGETS ${XrdClS3} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
