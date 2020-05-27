
#-------------------------------------------------------------------------------
# The XrdSecgsiVOMS library
#-------------------------------------------------------------------------------

include_directories( ${VOMS_INCLUDE_DIR} )

set( LIB_XRD_SEC_GSI_VOMS     XrdSecgsiVOMS-${PLUGIN_VERSION} )

add_library(
   ${LIB_XRD_SEC_GSI_VOMS}
   MODULE
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsFun.cc
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsgsi.cc
   ${CMAKE_SOURCE_DIR}/src/XrdVoms/XrdVomsHttp.cc )

target_link_libraries(
   ${LIB_XRD_SEC_GSI_VOMS}
   XrdCrypto
   ${VOMS_LIBRARIES} )

set_target_properties(
   ${LIB_XRD_SEC_GSI_VOMS}
   PROPERTIES
   INTERFACE_LINK_LIBRARIES ""
   LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
   TARGETS ${LIB_XRD_SEC_GSI_VOMS}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/libXrdSecgsiVOMS.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

