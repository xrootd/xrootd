
find_package( SciTokensCpp REQUIRED )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_SCITOKENS  XrdAccSciTokens-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# The XrdSciTokens object library
#
# This separate object library is created so unit tests can link directly to it
# (linking against a MODULE is forbidden)
#-------------------------------------------------------------------------------
add_library(
   XrdSciTokensObj
   OBJECT
   XrdSciTokens/XrdSciTokensAccess.cc
                                       XrdSciTokens/XrdSciTokensHelper.hh
   XrdSciTokens/XrdSciTokensMon.cc     XrdSciTokens/XrdSciTokensMon.hh )
target_link_libraries(
   XrdSciTokensObj
   PRIVATE
   ${SCITOKENS_CPP_LIBRARIES}
   XrdUtils
   XrdServer
   ${CMAKE_DL_LIBS}
   ${CMAKE_THREAD_LIBS_INIT} )

target_include_directories(
   XrdSciTokensObj
   PRIVATE
   ${SCITOKENS_CPP_INCLUDE_DIR}
   XrdSciTokens/vendor/picojson
   XrdSciTokens/vendor/inih )

set_target_properties(XrdSciTokensObj PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (HAVE_SCITOKEN_CONFIG_SET_STR)
   target_compile_definitions(XrdSciTokensObj PRIVATE HAVE_SCITOKEN_CONFIG_SET_STR)
endif()

#-------------------------------------------------------------------------------
# The XrdSciTokens module
#-------------------------------------------------------------------------------
add_library(
   ${LIB_XRD_SCITOKENS}
   MODULE
   "$<TARGET_OBJECTS:XrdSciTokensObj>")

target_link_libraries(${LIB_XRD_SCITOKENS} XrdSciTokensObj)

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
   TARGETS
   ${LIB_XRD_SCITOKENS}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
