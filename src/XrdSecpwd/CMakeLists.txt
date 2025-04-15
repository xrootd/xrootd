set(XrdSecpwd XrdSecpwd-${PLUGIN_VERSION})

add_library(${XrdSecpwd} MODULE
  XrdSecProtocolpwd.cc  XrdSecProtocolpwd.hh
                        XrdSecpwdPlatform.hh
)

target_link_libraries(${XrdSecpwd}
  PRIVATE
    XrdCrypto
    XrdUtils
    ${CRYPT_LIBRARY}
)

add_dependencies(plugins ${XrdSecpwd})

install(TARGETS ${XrdSecpwd} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(NOT XRDCL_LIB_ONLY)
  add_executable(xrdpwdadmin XrdSecpwdSrvAdmin.cc)
  target_link_libraries(xrdpwdadmin PRIVATE XrdCrypto XrdUtils)

  install(TARGETS xrdpwdadmin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
