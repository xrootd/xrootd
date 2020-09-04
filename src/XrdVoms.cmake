
#-------------------------------------------------------------------------------
# The XrdSecgsiVOMS library
#-------------------------------------------------------------------------------

include_directories( ${VOMS_INCLUDE_DIR} )

set( LIB_XRD_VOMS             XrdVoms-${PLUGIN_VERSION} )
set( LIB_XRD_SEC_GSI_VOMS     XrdSecgsiVOMS-${PLUGIN_VERSION} )
set( LIB_XRD_HTTP_VOMS        XrdHttpVOMS-${PLUGIN_VERSION} )

add_library(
   ${LIB_XRD_VOMS}
   MODULE
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsFun.cc
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsgsi.cc
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsHttp.cc )

target_link_libraries(
   ${LIB_XRD_VOMS}
   XrdCrypto
   ${VOMS_LIBRARIES} )

set_target_properties(
   ${LIB_XRD_VOMS}
   PROPERTIES
   INTERFACE_LINK_LIBRARIES ""
   LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
   TARGETS ${LIB_XRD_VOMS}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/libXrdVoms.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

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

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf libXrdVoms.1 libXrdSecgsiVOMS.1
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1 )" )
