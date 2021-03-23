include( XRootDCommon )
include( ExternalProject )

link_directories( ${ISAL_LIBDIR} )
include_directories( ${ISAL_INCDIR} )

#-------------------------------------------------------------------------------
# The XrdEc shared library
#-------------------------------------------------------------------------------

set( XRD_EC_VERSION   1.0.0 )
set( XRD_EC_SOVERSION 1 )

add_library(
  XrdEc
  SHARED
  XrdEcConfig.hh
  XrdEcObjCfg.hh
  XrdEcRedundancyProvider.hh   XrdEcRedundancyProvider.cc 
  XrdEcUtilities.hh            XrdEcUtilities.cc
  XrdEcWrtBuff.hh
  XrdEcThreadPool.hh
  XrdEcStrmWriter.hh           XrdEcStrmWriter.cc
  XrdEcReader.hh               XrdEcReader.cc
)

target_link_libraries(
  XrdEc
  XrdCl
  isal
)

set_target_properties(
  XrdEc
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES ""
  VERSION   ${XRD_EC_VERSION}
  SOVERSION ${XRD_EC_SOVERSION} )
  
add_dependencies( XrdEc isa-l )

#------------------------------------------------------------------------------
# Install XrdEc library
#------------------------------------------------------------------------------
install(
  TARGETS XrdEc
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

#------------------------------------------------------------------------------
# Install private header files
#------------------------------------------------------------------------------
install(
  FILES
    XrdEcReader.hh
    XrdEcObjCfg.hh
    XrdEcStrmWriter.hh
    XrdEcWrtBuff.hh
    XrdEcThreadPool.hh
    XrdEcUtilities.hh
    XrdEcObjCfg.hh
    XrdEcConfig.hh
    XrdEcRedundancyProvider.hh
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/private/XrdEc )
