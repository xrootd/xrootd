set(XrdOssSimulated XrdOssSimulated-${PLUGIN_VERSION})

add_library(${XrdOssSimulated} MODULE XrdOssSimulated.cc
    XrdOssSimulatedDir.cc
    XrdOssSimulatedFile.cc
    XrdOssSimulatedXAttr.cc
    )

install(TARGETS ${XrdOssSimulated} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
