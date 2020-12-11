include(GNUInstallDirs)

find_package( SciTokensCpp REQUIRED )

include_directories(${SCITOKENS_CPP_INCLUDE_DIR} vendor/picojson vendor/inih)

add_library(XrdAccSciTokens SHARED src/XrdAccSciTokens.cc src/XrdSciTokensHelper.hh)
target_link_libraries(XrdAccSciTokens -ldl -lpthread ${SCITOKENS_CPP_LIBRARIES} XrdUtils XrdServer)
set_target_properties(XrdAccSciTokens PROPERTIES OUTPUT_NAME XrdAccSciTokens-${PLUGIN_VERSION} SUFFIX ".so" LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/configs/export-lib-symbols")

SET(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Install path for libraries")

install(
  TARGETS XrdAccSciTokens
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
