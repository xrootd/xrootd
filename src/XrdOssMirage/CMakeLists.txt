set(XrdOssMirage XrdOssMirage-${PLUGIN_VERSION})

add_library(${XrdOssMirage} MODULE XrdOssMirage.cc
    XrdOssMirageDir.cc
    XrdOssMirageFile.cc
    XrdOssMirageXAttr.cc
    )

install(TARGETS ${XrdOssMirage} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
