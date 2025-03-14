unset(BUILD_MACAROONS CACHE)

if(NOT ENABLE_MACAROONS)
  return()
endif()

if( FORCE_ENABLED )
  if(NOT ENABLE_HTTP)
    message(SEND_ERROR "Cannot enable XrdMacaroons without HTTP support")
  endif()
  find_package(Macaroons REQUIRED)
else()
  find_package(Macaroons)
endif()

if(NOT APPLE)
  include(FindPkgConfig REQUIRED)
  if(FORCE_ENABLED)
    pkg_check_modules(JSON REQUIRED json-c)
  else()
    pkg_check_modules(JSON json-c)
  endif()
endif()

if(MACAROONS_FOUND AND JSON_FOUND AND BUILD_HTTP)
  set(BUILD_MACAROONS TRUE CACHE INTERNAL "")
else()
  return()
endif()

set(XrdMacaroons XrdMacaroons-${PLUGIN_VERSION})

add_library(${XrdMacaroons} MODULE
  XrdMacaroons.cc
  XrdMacaroonsAuthz.cc       XrdMacaroonsAuthz.hh
  XrdMacaroonsConfigure.cc
  XrdMacaroonsHandler.cc     XrdMacaroonsHandler.hh
)

target_link_libraries(${XrdMacaroons}
  PRIVATE
    XrdHttpUtils
    XrdUtils
    XrdServer
    uuid::uuid
    OpenSSL::Crypto
    ${MACAROONS_LIB}
    ${JSON_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

target_include_directories(${XrdMacaroons}
  PRIVATE
    ${JSON_INCLUDE_DIRS}
    ${MACAROONS_INCLUDES}
)

if(NOT APPLE)
  target_link_options(${XrdMacaroons} PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export-lib-symbols")
endif()

install(TARGETS ${XrdMacaroons} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
