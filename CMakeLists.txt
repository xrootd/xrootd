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
  XrdClPollerLibEvent.cc      XrdClPollerLibEvent.hh
  XrdClPostMaster.cc          XrdClPostMaster.hh
                              XrdClPostMasterInterfaces.hh
  XrdClChannel.cc             XrdClChannel.hh
  XrdClStream.cc              XrdClStream.hh
  XrdClXRootDTransport.cc     XrdClXRootDTransport.hh
  XrdClInQueue.cc             XrdClInQueue.hh
  XrdClTaskManager.cc         XrdClTaskManager.hh
  XrdClSIDManager.cc          XrdClSIDManager.hh
  XrdClQuery.cc               XrdClQuery.hh
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
# xrdquery
#-------------------------------------------------------------------------------
add_executable(
  xrdquery
  XrdClQueryCommandline.cc
  XrdClQueryExecutor.cc         XrdClQueryExecutor.hh )

target_link_libraries(
  xrdquery
  XrdCl
  ${READLINE_LIBRARY}
  ${NCURSES_LIBRARY} )

