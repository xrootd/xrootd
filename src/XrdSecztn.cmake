#-------------------------------------------------------------------------------
# The XrdSecztn module
#-------------------------------------------------------------------------------
set( LIB_XRD_SEC_ZTN XrdSecztn-${PLUGIN_VERSION} )

add_library(
  ${LIB_XRD_SEC_ZTN}
  MODULE
  XrdSecztn/XrdSecProtocolztn.cc
  XrdSecztn/XrdSecztn.cc )

target_link_libraries(
  ${LIB_XRD_SEC_ZTN}
  PRIVATE
  XrdUtils
  )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_SEC_ZTN}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
