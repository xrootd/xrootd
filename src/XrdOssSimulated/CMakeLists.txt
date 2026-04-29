set(XrdOssSimulated XrdOssSimulated-${PLUGIN_VERSION})

add_library(${XrdOssSimulated} MODULE XrdOssSimulated.cc
    XrdOssSimulatedDir.cc
    XrdOssSimulatedFile.cc
    )

install(TARGETS ${XrdOssSimulated} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(XrdOssSimulatedXAttr XrdOssSimulatedXAttr-${PLUGIN_VERSION})

add_library(${XrdOssSimulatedXAttr} MODULE XrdOssSimulatedXAttr.cc
    )

install(TARGETS ${XrdOssSimulatedXAttr} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
