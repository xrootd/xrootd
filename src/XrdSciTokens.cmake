
find_package( SciTokensCpp REQUIRED )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_SCITOKENS  XrdAccSciTokens-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# The XrdPfc library
#-------------------------------------------------------------------------------
add_library(
   ${LIB_XRD_SCITOKENS}
   MODULE
   XrdSciTokens/XrdSciTokensAccess.cc
   XrdMacaroons/XrdMacaroonsUtils.cc   XrdMacaroons/XrdMacaroonsUtils.hh
                                       XrdSciTokens/XrdSciTokensRedir.hh
                                       XrdSciTokens/XrdSciTokensHelper.hh
   XrdSciTokens/XrdSciTokensMon.cc     XrdSciTokens/XrdSciTokensMon.hh )
target_link_libraries(
   ${LIB_XRD_SCITOKENS}
   PRIVATE
   ${SCITOKENS_CPP_LIBRARIES}
   XrdUtils
   XrdServer
   ${CMAKE_DL_LIBS}
   ${CMAKE_THREAD_LIBS_INIT} )

target_include_directories(
   ${LIB_XRD_SCITOKENS}
   PRIVATE
   ${SCITOKENS_CPP_INCLUDE_DIR}
   XrdSciTokens/vendor/picojson
   XrdSciTokens/vendor/inih )

if( BUILD_MACAROONS )
   target_include_directories(
      ${LIB_XRD_SCITOKENS}
      PRIVATE
      ${MACAROONS_INCLUDES} )

   target_compile_definitions(${LIB_XRD_SCITOKENS} PRIVATE HAVE_MACAROONS)

   target_link_libraries(
       ${LIB_XRD_SCITOKENS}
       PRIVATE
       uuid::uuid
       OpenSSL::Crypto
       ${MACAROONS_LIB} )
endif()

if (HAVE_SCITOKEN_CONFIG_SET_STR)
   target_compile_definitions(${LIB_XRD_SCITOKENS} PRIVATE HAVE_SCITOKEN_CONFIG_SET_STR)
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
   TARGETS
   ${LIB_XRD_SCITOKENS}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
