
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
                                       XrdSciTokens/XrdSciTokensHelper.hh )
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

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
   TARGETS
   ${LIB_XRD_SCITOKENS}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
