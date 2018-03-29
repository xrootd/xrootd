
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CLIENT_VERSION   2.0.0 )
set( XRD_CLIENT_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdClient lib
#-------------------------------------------------------------------------------
add_library(
  XrdClient
  SHARED
  XrdClient/XrdClientAbs.cc            XrdClient/XrdClientAbs.hh
  XrdClient/XrdClient.cc               XrdClient/XrdClient.hh
  XrdClient/XrdClientSock.cc           XrdClient/XrdClientSock.hh
  XrdClient/XrdClientPSock.cc          XrdClient/XrdClientPSock.hh
  XrdClient/XrdClientConn.cc           XrdClient/XrdClientConn.hh
  XrdClient/XrdClientConnMgr.cc        XrdClient/XrdClientConnMgr.hh
                                       XrdClient/XrdClientUnsolMsg.hh
  XrdClient/XrdClientDebug.cc          XrdClient/XrdClientDebug.hh
  XrdClient/XrdClientInputBuffer.cc    XrdClient/XrdClientInputBuffer.hh
  XrdClient/XrdClientLogConnection.cc  XrdClient/XrdClientLogConnection.hh
  XrdClient/XrdClientPhyConnection.cc  XrdClient/XrdClientPhyConnection.hh
  XrdClient/XrdClientMessage.cc        XrdClient/XrdClientMessage.hh
  XrdClient/XrdClientProtocol.cc       XrdClient/XrdClientProtocol.hh
  XrdClient/XrdClientReadCache.cc      XrdClient/XrdClientReadCache.hh
  XrdClient/XrdClientUrlInfo.cc        XrdClient/XrdClientUrlInfo.hh
  XrdClient/XrdClientUrlSet.cc         XrdClient/XrdClientUrlSet.hh
  XrdClient/XrdClientThread.cc         XrdClient/XrdClientThread.hh
  XrdClient/XrdClientAdmin.cc          XrdClient/XrdClientAdmin.hh
                                       XrdClient/XrdClientVector.hh
  XrdClient/XrdClientEnv.cc            XrdClient/XrdClientEnv.hh
                                       XrdClient/XrdClientConst.hh
  XrdClient/XrdClientSid.cc            XrdClient/XrdClientSid.hh
  XrdClient/XrdClientMStream.cc        XrdClient/XrdClientMStream.hh
  XrdClient/XrdClientReadV.cc          XrdClient/XrdClientReadV.hh
  XrdClient/XrdClientReadAhead.cc      XrdClient/XrdClientReadAhead.hh )

target_link_libraries(
  XrdClient
  XrdUtils
  dl
  pthread )

set_target_properties(
  XrdClient
  PROPERTIES
  VERSION   ${XRD_CLIENT_VERSION}
  SOVERSION ${XRD_CLIENT_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# xrd
#-------------------------------------------------------------------------------
add_executable(
  xrd
  XrdClient/XrdCommandLine.cc )

target_link_libraries(
  xrd
  XrdClient
  XrdUtils
  pthread
  ${READLINE_LIBRARY}
  ${NCURSES_LIBRARY} )

#-------------------------------------------------------------------------------
# xprep
#-------------------------------------------------------------------------------
add_executable(
  xprep
  XrdClient/XrdClientPrep.cc )

target_link_libraries(
  xprep
  XrdClient
  XrdUtils
  pthread )

#-------------------------------------------------------------------------------
# xrdstagetool
#-------------------------------------------------------------------------------
add_executable(
  xrdstagetool
  XrdClient/XrdStageTool.cc )

target_link_libraries(
  xrdstagetool
  XrdClient
  XrdUtils
  pthread )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdClient xrd xprep xrdstagetool
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrd.1
  ${PROJECT_SOURCE_DIR}/docs/man/xprep.1
  ${PROJECT_SOURCE_DIR}/docs/man/xrdstagetool.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

# FIXME: files
#-rw-r--r-- 1 ljanyst ljanyst  1643 2011-03-21 16:13 TestXrdClient.cc
#-rw-r--r-- 1 ljanyst ljanyst 20576 2011-03-21 16:13 TestXrdClient_read.cc
#-rwxr-xr-x 1 ljanyst ljanyst 15314 2011-03-21 16:13 xrdadmin
#-rw-r--r-- 1 ljanyst ljanyst  1516 2011-03-21 16:13 XrdClientAbsMonIntf.hh
#-rw-r--r-- 1 ljanyst ljanyst  1026 2011-03-21 16:13 XrdClientCallback.hh
