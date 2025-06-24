set(XrdCors       XrdCors-${PLUGIN_VERSION})

add_library(${XrdCors} SHARED
        XrdCorsHandler.cc
        )

install(TARGETS ${XrdCors} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})