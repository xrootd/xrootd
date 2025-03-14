set(XrdSecsss XrdSecsss-${PLUGIN_VERSION})

add_library(${XrdSecsss} MODULE
  XrdSecProtocolsss.cc   XrdSecProtocolsss.hh
                         XrdSecsssRR.hh
)

target_link_libraries(${XrdSecsss} PRIVATE XrdCryptoLite XrdUtils)

add_dependencies(plugins ${XrdSecsss})

install(TARGETS ${XrdSecsss} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(NOT XRDCL_LIB_ONLY)
  add_executable(xrdsssadmin XrdSecsssAdmin.cc)
  target_link_libraries(xrdsssadmin XrdUtils)

  install(TARGETS xrdsssadmin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
