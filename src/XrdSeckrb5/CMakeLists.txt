if(NOT BUILD_KRB5)
  return()
endif()

set(XrdSeckrb5 XrdSeckrb5-${PLUGIN_VERSION})

add_library(${XrdSeckrb5} MODULE XrdSecProtocolkrb5.cc)

target_link_libraries(${XrdSeckrb5} PRIVATE XrdUtils ${KERBEROS5_LIBRARIES})
target_include_directories(${XrdSeckrb5} PRIVATE ${KERBEROS5_INCLUDE_DIR})

install(TARGETS ${XrdSeckrb5} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
