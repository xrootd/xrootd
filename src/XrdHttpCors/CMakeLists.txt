set(XrdHttpCors       XrdHttpCors-${PLUGIN_VERSION})

add_library(${XrdHttpCors} MODULE XrdHttpCorsHandler.cc)
target_link_libraries(${XrdHttpCors} XrdUtils )

install(TARGETS ${XrdHttpCors} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})