set(XrdSimulated XrdSimulated-${PLUGIN_VERSION})

add_library(${XrdSimulated} MODULE XrdSimulated.cc
    XrdSimulatedDir.cc
    XrdSimulatedFile.cc
    )

install(TARGETS ${XrdSimulated} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
