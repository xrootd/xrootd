
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Modules
#-------------------------------------------------------------------------------
set( LIB_XRD_BWM        XrdBwm-${PLUGIN_VERSION} )
set( LIB_XRD_N2NO2P     XrdN2No2p-${PLUGIN_VERSION} )
set( LIB_XRD_PSS        XrdPss-${PLUGIN_VERSION} )
set( LIB_XRD_CMSREDIRL  XrdCmsRedirectLocal-${PLUGIN_VERSION} )
set( LIB_XRD_GPFS       XrdOssSIgpfsT-${PLUGIN_VERSION} )
set( LIB_XRD_ZCRC32     XrdCksCalczcrc32-${PLUGIN_VERSION} )
set( LIB_XRD_THROTTLE   XrdThrottle-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# The XrdPss module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_PSS}
  MODULE
  XrdPss/XrdPssAio.cc
  XrdPss/XrdPssAioCB.cc      XrdPss/XrdPssAioCB.hh
  XrdPss/XrdPss.cc           XrdPss/XrdPss.hh
  XrdPss/XrdPssCks.cc        XrdPss/XrdPssCks.hh
  XrdPss/XrdPssConfig.cc
                             XrdPss/XrdPssTrace.hh
  XrdPss/XrdPssUrlInfo.cc    XrdPss/XrdPssUrlInfo.hh
  XrdPss/XrdPssUtils.cc      XrdPss/XrdPssUtils.hh )

target_link_libraries(
  ${LIB_XRD_PSS}
  XrdPosix
  XrdUtils
  XrdServer )

set_target_properties(
  ${LIB_XRD_PSS}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdBwm module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_BWM}
  MODULE
  XrdBwm/XrdBwm.cc             XrdBwm/XrdBwm.hh
  XrdBwm/XrdBwmConfig.cc
  XrdBwm/XrdBwmHandle.cc       XrdBwm/XrdBwmHandle.hh
  XrdBwm/XrdBwmLogger.cc       XrdBwm/XrdBwmLogger.hh
  XrdBwm/XrdBwmPolicy1.cc      XrdBwm/XrdBwmPolicy1.hh
                               XrdBwm/XrdBwmPolicy.hh
                               XrdBwm/XrdBwmTrace.hh )

target_link_libraries(
  ${LIB_XRD_BWM}
  XrdServer
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT} )

set_target_properties(
  ${LIB_XRD_BWM}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# N2No2p plugin library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_N2NO2P}
  MODULE
  XrdOuc/XrdOucN2No2p.cc )

target_link_libraries(
  ${LIB_XRD_N2NO2P}
  XrdUtils )

set_target_properties(
  ${LIB_XRD_N2NO2P}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# GPFS stat() plugin library
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_GPFS}
  MODULE
  XrdOss/XrdOssSIgpfsT.cc )

target_link_libraries(
  ${LIB_XRD_GPFS}
  XrdUtils )

set_target_properties(
  ${LIB_XRD_GPFS}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# libz compatible CRC32 plugin
#-------------------------------------------------------------------------------

add_library(
  ${LIB_XRD_ZCRC32}
  MODULE
  XrdCks/XrdCksCalczcrc32.cc )

target_link_libraries(
  ${LIB_XRD_ZCRC32}
  XrdUtils
  ${ZLIB_LIBRARIES} )

set_target_properties(
  ${LIB_XRD_ZCRC32}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdThrottle lib
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_THROTTLE}
  MODULE
  XrdOfs/XrdOfsFS.cc
  XrdThrottle/XrdThrottle.hh           XrdThrottle/XrdThrottleTrace.hh
  XrdThrottle/XrdThrottleFileSystem.cc
  XrdThrottle/XrdThrottleFileSystemConfig.cc
  XrdThrottle/XrdThrottleFile.cc
  XrdThrottle/XrdThrottleManager.cc    XrdThrottle/XrdThrottleManager.hh
)

target_link_libraries(
  ${LIB_XRD_THROTTLE}
  XrdServer
  XrdUtils )

set_target_properties(
  ${LIB_XRD_THROTTLE}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XrdCmsRedirLocal module
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_CMSREDIRL}
  MODULE
  XrdCms/XrdCmsRedirLocal.cc XrdCms/XrdCmsRedirLocal.hh )

target_link_libraries(
  ${LIB_XRD_CMSREDIRL}
  XrdServer
  XrdUtils )

set_target_properties(
  ${LIB_XRD_CMSREDIRL}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS ${LIB_XRD_PSS} ${LIB_XRD_BWM} ${LIB_XRD_GPFS} ${LIB_XRD_ZCRC32} ${LIB_XRD_THROTTLE} ${LIB_XRD_N2NO2P} ${LIB_XRD_CMSREDIRL}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
