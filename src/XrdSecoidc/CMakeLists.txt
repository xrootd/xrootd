set(XrdSecoidc XrdSecoidc-${PLUGIN_VERSION})

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(${XrdSecoidc} MODULE XrdSecProtocoloidc.cc)
target_link_libraries(${XrdSecoidc} PRIVATE XrdUtils OpenSSL::Crypto CURL::libcurl)

install(TARGETS ${XrdSecoidc} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
