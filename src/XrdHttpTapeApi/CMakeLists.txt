if(NOT BUILD_HTTP)
  return()
endif()

set(XrdHttpTapeApi XrdHttpTapeApi-${PLUGIN_VERSION})

add_library(${XrdHttpTapeApi} MODULE
  XrdHttpTapeApi.cc
)

target_link_libraries(${XrdHttpTapeApi}
  PRIVATE
    XrdHttpUtils
)

if(NOT APPLE)
  target_link_options(${XrdHttpTapeApi} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export-lib-symbols")
endif()

install(TARGETS ${XrdHttpTapeApi} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
