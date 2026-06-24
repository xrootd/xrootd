if(NOT BUILD_HTTP)
  return()
endif()

set(XrdHttpPrometheus XrdHttpPrometheus-${PLUGIN_VERSION})

add_library(${XrdHttpPrometheus} MODULE XrdHttpPrometheus.cc XrdHttpPrometheus.hh)
target_link_libraries(${XrdHttpPrometheus} XrdUtils XrdHttpUtils)

# Optional Pushgateway support needs libcurl.
find_package(CURL)
if(CURL_FOUND)
  target_compile_definitions(${XrdHttpPrometheus} PRIVATE XRDPROM_HAVE_CURL)
  target_link_libraries(${XrdHttpPrometheus} CURL::libcurl)
endif()

install(TARGETS ${XrdHttpPrometheus} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
