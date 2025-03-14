set(XrdBwm XrdBwm-${PLUGIN_VERSION})

add_library(${XrdBwm} MODULE
  XrdBwm.cc         XrdBwm.hh
  XrdBwmConfig.cc
  XrdBwmHandle.cc   XrdBwmHandle.hh
  XrdBwmLogger.cc   XrdBwmLogger.hh
  XrdBwmPolicy1.cc  XrdBwmPolicy1.hh
                    XrdBwmPolicy.hh
                    XrdBwmTrace.hh
)

target_link_libraries(${XrdBwm}
  PRIVATE
    XrdServer
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS ${XrdBwm} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
