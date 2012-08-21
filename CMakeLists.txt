if( LIBEVENT_FOUND )
  include_directories( ${LIBEVENT_INCLUDE_DIR} )
endif()

include_directories( ../ ${XROOTD_INCLUDES} ${LIBEVENT_INCLUDE_DIR} )
link_directories( ${XROOTD_LIB_DIR} )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CL_VERSION   0.0.1 )
set( XRD_CL_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdCl lib
#-------------------------------------------------------------------------------
add_library(
  XrdCl
  SHARED
  XrdClLog.cc                 XrdClLog.hh
  XrdClUtils.cc               XrdClUtils.hh
                              XrdClOptimizers.hh
                              XrdClConstants.hh
  XrdClEnv.cc                 XrdClEnv.hh
  XrdClDefaultEnv.cc          XrdClDefaultEnv.hh
  XrdClURL.cc                 XrdClURL.hh
  XrdClStatus.cc              XrdClStatus.hh
  XrdClSocket.cc              XrdClSocket.hh
                              XrdClPoller.hh
  XrdClPollerFactory.cc       XrdClPollerFactory.hh
  XrdClPollerBuiltIn.cc       XrdClPollerBuiltIn.hh
  XrdClPostMaster.cc          XrdClPostMaster.hh
                              XrdClPostMasterInterfaces.hh
  XrdClChannel.cc             XrdClChannel.hh
  XrdClStream.cc              XrdClStream.hh
  XrdClXRootDTransport.cc     XrdClXRootDTransport.hh
  XrdClInQueue.cc             XrdClInQueue.hh
  XrdClOutQueue.cc            XrdClOutQueue.hh
  XrdClTaskManager.cc         XrdClTaskManager.hh
  XrdClSIDManager.cc          XrdClSIDManager.hh
  XrdClFileSystem.cc          XrdClFileSystem.hh
  XrdClXRootDMsgHandler.cc    XrdClXRootDMsgHandler.hh
                              XrdClBuffer.hh
                              XrdClMessage.hh
  XrdClMessageUtils.cc        XrdClMessageUtils.hh
  XrdClXRootDResponses.cc     XrdClXRootDResponses.hh
                              XrdClRequestSync.hh
  XrdClFile.cc                XrdClFile.hh
  XrdClFileStateHandler.cc    XrdClFileStateHandler.hh
  XrdClCopyProcess.cc         XrdClCopyProcess.hh
  XrdClClassicCopyJob.cc      XrdClClassicCopyJob.hh
  XrdClThirdPartyCopyJob.cc   XrdClThirdPartyCopyJob.hh
  XrdClAsyncSocketHandler.cc  XrdClAsyncSocketHandler.hh
  ${LIBEVENT_POLLER_FILES}
)

target_link_libraries(
  XrdCl
  XrdUtils
  pthread
  dl
  ${LIBEVENT_LIB}
  ${LIBEVENTPTHREADS_LIB})

set_target_properties(
  XrdCl
  PROPERTIES
  VERSION   ${XRD_CL_VERSION}
  SOVERSION ${XRD_CL_SOVERSION} )

#-------------------------------------------------------------------------------
# xrdfs
#-------------------------------------------------------------------------------
add_executable(
  xrdfs
  XrdClFS.cc
  XrdClFSExecutor.cc         XrdClFSExecutor.hh )

target_link_libraries(
  xrdfs
  XrdCl
  ${READLINE_LIBRARY}
  ${NCURSES_LIBRARY} )

#-------------------------------------------------------------------------------
# xrdcopy
#-------------------------------------------------------------------------------
add_executable(
  xrdcopy
  XrdClCopy.cc )

target_link_libraries(
  xrdcopy
  XrdCl
  XrdAppUtils )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCl xrdfs xrdcopy
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      ./
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdCl
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
