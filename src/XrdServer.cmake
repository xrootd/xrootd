
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Plugin version (this protocol loaded eithr as a plugin or as builtin).
#-------------------------------------------------------------------------------
set( LIB_XRD_PROTOCOL XrdXrootd-${PLUGIN_VERSION} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SERVER_VERSION   2.0.0 )
set( XRD_SERVER_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XRootD protocol implementation
#-------------------------------------------------------------------------------
add_library(
  XrdServer
  SHARED
  XrdXrootd/XrdXrootdAdmin.cc           XrdXrootd/XrdXrootdAdmin.hh
  XrdXrootd/XrdXrootdAio.cc             XrdXrootd/XrdXrootdAio.hh
  XrdXrootd/XrdXrootdBridge.cc          XrdXrootd/XrdXrootdBridge.hh
  XrdXrootd/XrdXrootdCallBack.cc        XrdXrootd/XrdXrootdCallBack.hh
  XrdXrootd/XrdXrootdConfig.cc
  XrdXrootd/XrdXrootdFile.cc            XrdXrootd/XrdXrootdFile.hh
                                        XrdXrootd/XrdXrootdFileLock.hh
  XrdXrootd/XrdXrootdFileLock1.cc       XrdXrootd/XrdXrootdFileLock1.hh
                                        XrdXrootd/XrdXrootdFileStats.hh
  XrdXrootd/XrdXrootdJob.cc             XrdXrootd/XrdXrootdJob.hh
  XrdXrootd/XrdXrootdLoadLib.cc
                                        XrdXrootd/XrdXrootdMonData.hh
  XrdXrootd/XrdXrootdMonFile.cc         XrdXrootd/XrdXrootdMonFile.hh
  XrdXrootd/XrdXrootdMonFMap.cc         XrdXrootd/XrdXrootdMonFMap.hh
  XrdXrootd/XrdXrootdMonitor.cc         XrdXrootd/XrdXrootdMonitor.hh

  XrdXrootd/XrdXrootdPio.cc             XrdXrootd/XrdXrootdPio.hh
  XrdXrootd/XrdXrootdPrepare.cc         XrdXrootd/XrdXrootdPrepare.hh
  XrdXrootd/XrdXrootdProtocol.cc        XrdXrootd/XrdXrootdProtocol.hh
  XrdXrootd/XrdXrootdResponse.cc        XrdXrootd/XrdXrootdResponse.hh
                                        XrdXrootd/XrdXrootdStat.icc
  XrdXrootd/XrdXrootdStats.cc           XrdXrootd/XrdXrootdStats.hh
  XrdXrootd/XrdXrootdTransit.cc         XrdXrootd/XrdXrootdTransit.hh
  XrdXrootd/XrdXrootdTransPend.cc       XrdXrootd/XrdXrootdTransPend.hh
  XrdXrootd/XrdXrootdTransSend.cc       XrdXrootd/XrdXrootdTransSend.hh
  XrdXrootd/XrdXrootdXeq.cc
  XrdXrootd/XrdXrootdXeqAio.cc
                                        XrdXrootd/XrdXrootdTrace.hh
                                        XrdXrootd/XrdXrootdXPath.hh
                                        XrdXrootd/XrdXrootdReqID.hh )

target_link_libraries(
  XrdServer
  XrdOfs
  XrdUtils
  dl
  pthread
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY} )

set_target_properties(
  XrdServer
  PROPERTIES
  VERSION   ${XRD_SERVER_VERSION}
  SOVERSION ${XRD_SERVER_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# The XRootD protocol plugin
#-------------------------------------------------------------------------------
add_library(
  ${LIB_XRD_PROTOCOL}
  MODULE
  XrdXrootd/XrdXrootdPlugin.cc )

target_link_libraries(
  ${LIB_XRD_PROTOCOL}
  XrdServer
  XrdUtils
  ${EXTRA_LIBS} )

set_target_properties(
  ${LIB_XRD_PROTOCOL}
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdServer ${LIB_XRD_PROTOCOL}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
