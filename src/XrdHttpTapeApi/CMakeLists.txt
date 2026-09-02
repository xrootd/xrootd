if(NOT BUILD_HTTP)
  return()
endif()

set(XrdHttpTapeApi XrdHttpTapeApi-${PLUGIN_VERSION})

add_library(XrdHttpTapeApiStore STATIC
  XrdHttpTapeApiStore.cc
)

set_target_properties(XrdHttpTapeApiStore
  PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(XrdHttpTapeApiStore
  PUBLIC
    XrdUtils
  PRIVATE
    uuid::uuid
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(XrdHttpTapeApiStore PUBLIC stdc++fs)
endif()

add_library(${XrdHttpTapeApi} MODULE
  XrdHttpTapeApi.cc
)

target_link_libraries(${XrdHttpTapeApi}
  PRIVATE
    XrdHttpUtils
    XrdHttpTapeApiStore
)

if(NOT APPLE)
  target_link_options(${XrdHttpTapeApi} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export-lib-symbols")
endif()

install(TARGETS ${XrdHttpTapeApi} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
