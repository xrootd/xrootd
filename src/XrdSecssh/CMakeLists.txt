set(XrdSecssh XrdSecssh-${PLUGIN_VERSION})

find_package(OpenSSL REQUIRED)

add_library(${XrdSecssh} MODULE XrdSecProtocolssh.cc)
target_link_libraries(${XrdSecssh} PRIVATE XrdUtils OpenSSL::Crypto)
add_dependencies(plugins ${XrdSecssh})

install(TARGETS ${XrdSecssh} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
