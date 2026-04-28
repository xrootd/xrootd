set(XrdSimulated XrdSimulated-${PLUGIN_VERSION})

add_library(${XrdSimulated} MODULE XrdSimulated.cc
    XrdSimulatedDir.cc
    XrdSimulatedFile.cc
    )

install(TARGETS ${XrdSimulated} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(XrdSimulatedXAttr XrdSimulatedXAttr-${PLUGIN_VERSION})

add_library(${XrdSimulatedXAttr} MODULE XrdSimulatedXAttr.cc
    )

install(TARGETS ${XrdSimulatedXAttr} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
