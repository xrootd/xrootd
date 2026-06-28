if(NOT BUILD_HTTP)
  return()
endif()

set(XrdHttpMetricsExporter XrdHttpMetricsExporter-${PLUGIN_VERSION})

add_library(${XrdHttpMetricsExporter} MODULE
  XrdHttpMetricsExporter.cc XrdHttpMetricsExporter.hh
  XrdHttpMetricsExporterPush.hh)
target_link_libraries(${XrdHttpMetricsExporter} XrdUtils XrdHttpUtils)

# Optional push support (Pushgateway / OTLP) needs libcurl.
find_package(CURL)
if(CURL_FOUND)
  target_compile_definitions(${XrdHttpMetricsExporter} PRIVATE XRDMETRICS_HAVE_CURL)
  target_link_libraries(${XrdHttpMetricsExporter} CURL::libcurl)
endif()

install(TARGETS ${XrdHttpMetricsExporter} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
