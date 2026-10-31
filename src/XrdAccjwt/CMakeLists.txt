unset(BUILD_ACCJWT CACHE)

if(XRDCL_ONLY)
  return()
endif()

set(BUILD_ACCJWT TRUE CACHE INTERNAL "")

set(XrdAccjwt XrdAccjwt-${PLUGIN_VERSION})

add_library(${XrdAccjwt} MODULE
  XrdAccjwtAccess.cc
  XrdAccjwtScope.cc
)

target_include_directories(${XrdAccjwt}
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${XrdAccjwt}
  PRIVATE
    XrdUtils
    XrdServer
    ${CMAKE_DL_LIBS}
)

if(TARGET XrdSecjwtCore)
  target_link_libraries(${XrdAccjwt} PRIVATE XrdSecjwtCore)
  target_compile_definitions(${XrdAccjwt} PRIVATE XRDACCJWT_JWT=1)
endif()

if(NOT APPLE)
  target_link_options(${XrdAccjwt} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export-lib-symbols")
endif()

install(TARGETS ${XrdAccjwt} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
