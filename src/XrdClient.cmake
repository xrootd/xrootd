
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CLIENT_VERSION   0.0.1 )
set( XRD_CLIENT_SOVERSION 0 )

set( XRD_CLIENT_ADMIN_VERSION   0.0.1 )
set( XRD_CLIENT_ADMIN_SOVERSION 0 )

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
  XrdUtils )

set_target_properties(
  XrdClient
  PROPERTIES
  VERSION   ${XRD_CLIENT_VERSION}
  SOVERSION ${XRD_CLIENT_SOVERSION} )

#-------------------------------------------------------------------------------
# xrdcp
#-------------------------------------------------------------------------------
add_executable(
  xrdcp
  XrdClient/Xrdcp.cc
  XrdClient/XrdcpXtremeRead.cc         XrdClient/XrdcpXtremeRead.hh
  XrdClient/XrdCpMthrQueue.cc          XrdClient/XrdCpMthrQueue.hh
  XrdClient/XrdCpWorkLst.cc            XrdClient/XrdCpWorkLst.hh )

target_link_libraries(
  xrdcp
  XrdClient
  XrdCrypto
  dl
  ${ZLIB_LIBRARY} )

#-------------------------------------------------------------------------------
# xrd
#-------------------------------------------------------------------------------
add_executable(
  xrd
  XrdClient/XrdCommandLine.cc )

target_link_libraries(
  xrd
  XrdClient
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
  XrdClient )

#-------------------------------------------------------------------------------
# xrdstagetool
#-------------------------------------------------------------------------------
add_executable(
  xrdstagetool
  XrdClient/XrdStageTool.cc )

target_link_libraries(
  xrdstagetool
  XrdClient )

#-------------------------------------------------------------------------------
# Perl bindings
#-------------------------------------------------------------------------------
if( PERLLIBS_FOUND )
  include_directories( ${PERL_INCLUDE_PATH} )

  #-----------------------------------------------------------------------------
  # Check if we have the right version of SWIG
  #-----------------------------------------------------------------------------
  set( USE_SWIG FALSE )
  if( SWIG_FOUND )
    if( ${SWIG_VERSION} VERSION_GREATER "1.3.33" )
      set( USE_SWIG TRUE )
    endif()
  endif()

  #-----------------------------------------------------------------------------
  # We have SWIG
  #-----------------------------------------------------------------------------
  if( USE_SWIG )
    add_custom_command(
      OUTPUT XrdClientAdmin_c_wrap.cc XrdClientAdmin.pm
      COMMAND
      ${SWIG_EXECUTABLE} -c++ -perl -o XrdClientAdmin_c_wrap.cc
      ${CMAKE_SOURCE_DIR}/src/XrdClient/XrdClientAdmin_c.hh
      MAIN_DEPENDENCY XrdClient/XrdClientAdmin_c.hh )

  #-----------------------------------------------------------------------------
  # No SWIG
  #-----------------------------------------------------------------------------
  else()
    add_custom_command(
      OUTPUT XrdClientAdmin_c_wrap.cc XrdClientAdmin.pm
      COMMAND
      cp ${CMAKE_SOURCE_DIR}/src/XrdClient/XrdClientAdmin_c_wrap.c
      XrdClientAdmin_c_wrap.cc
      COMMAND
      cp ${CMAKE_SOURCE_DIR}/src/XrdClient/XrdClientAdmin.pm . )
  endif()

  add_library(
    XrdClientAdmin
    SHARED
    XrdClientAdmin_c_wrap.cc
    XrdClient/XrdClientAdmin_c.cc XrdClient/XrdClientAdmin_c.hh )

  target_link_libraries(
    XrdClientAdmin
    XrdClient
    ${PERL_LIBRARY} )

  set_target_properties(
    XrdClientAdmin
    PROPERTIES
    VERSION   ${XRD_CLIENT_ADMIN_VERSION}
    SOVERSION ${XRD_CLIENT_ADMIN_SOVERSION} )

endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdClient xrdcp xrd xprep xrdstagetool
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdcp.1
  ${PROJECT_SOURCE_DIR}/docs/man/xrd.1
  ${PROJECT_SOURCE_DIR}/docs/man/xprep.1
  ${PROJECT_SOURCE_DIR}/docs/man/xrdstagetool.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

install(
  DIRECTORY      XrdClient/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdClient
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )

#-------------------------------------------------------------------------------
# Install the perl bindings
#-------------------------------------------------------------------------------
if( PERLLIBS_FOUND )
  install(
    TARGETS XrdClientAdmin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

  install(
    FILES
    ${PROJECT_BINARY_DIR}/src/XrdClientAdmin.pm
    DESTINATION ${CMAKE_INSTALL_LIBDIR} )
endif()

# FIXME: files
#-rw-r--r-- 1 ljanyst ljanyst  1643 2011-03-21 16:13 TestXrdClient.cc
#-rw-r--r-- 1 ljanyst ljanyst 20576 2011-03-21 16:13 TestXrdClient_read.cc
#-rwxr-xr-x 1 ljanyst ljanyst  1848 2011-03-21 16:13 tinytestXTNetAdmin.pl
#-rwxr-xr-x 1 ljanyst ljanyst 15314 2011-03-21 16:13 xrdadmin
#-rw-r--r-- 1 ljanyst ljanyst  1516 2011-03-21 16:13 XrdClientAbsMonIntf.hh
#-rw-r--r-- 1 ljanyst ljanyst 18822 2011-03-21 16:13 XrdClientAdminJNI.cc
#-rw-r--r-- 1 ljanyst ljanyst  3239 2011-03-21 16:13 XrdClientAdminJNI.h
#-rw-r--r-- 1 ljanyst ljanyst  1636 2011-03-21 16:13 XrdClientAdminJNI.java
#-rw-r--r-- 1 ljanyst ljanyst  1026 2011-03-21 16:13 XrdClientCallback.hh
