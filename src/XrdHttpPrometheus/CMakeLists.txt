if(NOT BUILD_HTTP)
  return()
endif()

set(XrdHttpPrometheus XrdHttpPrometheus-${PLUGIN_VERSION})

add_library(${XrdHttpPrometheus} MODULE XrdHttpPrometheus.cc XrdHttpPrometheus.hh)
target_link_libraries(${XrdHttpPrometheus} XrdUtils XrdHttpUtils)

install(TARGETS ${XrdHttpPrometheus} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
