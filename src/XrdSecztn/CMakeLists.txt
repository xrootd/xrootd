set(XrdSecztn XrdSecztn-${PLUGIN_VERSION})

add_library(${XrdSecztn} MODULE XrdSecProtocolztn.cc XrdSecztn.cc)
target_link_libraries(${XrdSecztn} PRIVATE XrdUtils)

install(TARGETS ${XrdSecztn} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
