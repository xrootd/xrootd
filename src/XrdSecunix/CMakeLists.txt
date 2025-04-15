set(XrdSecunix XrdSecunix-${PLUGIN_VERSION})

add_library(${XrdSecunix} MODULE XrdSecProtocolunix.cc)
target_link_libraries(${XrdSecunix} PRIVATE XrdUtils)

add_dependencies(plugins ${XrdSecunix})

install(TARGETS ${XrdSecunix} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
