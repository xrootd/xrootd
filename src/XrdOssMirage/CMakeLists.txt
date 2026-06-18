set(XrdOssMirage XrdOssMirage-${PLUGIN_VERSION})

add_library(${XrdOssMirage} MODULE XrdOssMirage.cc
    XrdOssMirageDir.cc
    XrdOssMirageFile.cc
    XrdOssMirageXAttr.cc
    )

target_link_libraries(${XrdOssMirage} PRIVATE XrdUtils XrdServer)

install(TARGETS ${XrdOssMirage} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
