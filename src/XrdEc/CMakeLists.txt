include( ExternalProject )

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

target_link_libraries(XrdEc PRIVATE XrdCl XrdUtils ${ISAL_LIBRARIES})
target_include_directories(XrdEc PRIVATE ${ISAL_INCLUDE_DIRS})

set_target_properties(
  XrdEc
  PROPERTIES
  VERSION   ${XRD_EC_VERSION}
  SOVERSION ${XRD_EC_SOVERSION} )
  
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
if (USE_SYSTEM_ISAL)
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
endif()
