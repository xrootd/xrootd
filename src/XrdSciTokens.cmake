include( XRootDCommon )

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
   ${SCITOKENS_CPP_LIBRARIES}
   XrdUtils
   XrdServer
   ${CMAKE_DL_LIBS}
   ${CMAKE_THREAD_LIBS_INIT} )

set_target_properties(
   ${LIB_XRD_SCITOKENS}
   PROPERTIES
   INTERFACE_LINK_LIBRARIES ""
   LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
   TARGETS
   ${LIB_XRD_SCITOKENS}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
