
#-------------------------------------------------------------------------------
# The XrdSecgsiVOMS library
#-------------------------------------------------------------------------------

set( LIB_XRD_VOMS             XrdVoms-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_GSI_VOMS     XrdSecgsiVOMS-${PLUGIN_VERSION} )
set( LIB_XRD_HTTP_VOMS        XrdHttpVOMS-${PLUGIN_VERSION} )

add_library(
   ${LIB_XRD_VOMS}
   MODULE
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsFun.cc
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsMapfile.cc
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsgsi.cc
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsHttp.cc )

target_link_libraries(
   ${LIB_XRD_VOMS}
   PRIVATE
   XrdCrypto
   ${VOMS_LIBRARIES} )

target_include_directories( ${LIB_XRD_VOMS} PRIVATE ${VOMS_INCLUDE_DIR} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
   TARGETS ${LIB_XRD_VOMS}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf lib${LIB_XRD_VOMS}.so lib${LIB_XRD_SEC_GSI_VOMS}.so
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} )" )

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf lib${LIB_XRD_VOMS}.so lib${LIB_XRD_HTTP_VOMS}.so
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} )" )
