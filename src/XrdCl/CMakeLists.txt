if(NOT ENABLE_XRDCL)
  return()
endif()

add_library(XrdCl SHARED
  XrdClLog.cc                    XrdClLog.hh
  XrdClUtils.cc                  XrdClUtils.hh
                                 XrdClOptimizers.hh
                                 XrdClConstants.hh
  XrdClEnv.cc                    XrdClEnv.hh
  XrdClDefaultEnv.cc             XrdClDefaultEnv.hh
  XrdClURL.cc                    XrdClURL.hh
  XrdClStatus.cc                 XrdClStatus.hh
  XrdClSocket.cc                 XrdClSocket.hh
  XrdClTls.cc                    XrdClTls.hh
                                 XrdClPoller.hh
  XrdClPollerFactory.cc          XrdClPollerFactory.hh
  XrdClPollerBuiltIn.cc          XrdClPollerBuiltIn.hh
  XrdClPostMaster.cc             XrdClPostMaster.hh
                                 XrdClPostMasterInterfaces.hh
  XrdClChannel.cc                XrdClChannel.hh
  XrdClStream.cc                 XrdClStream.hh
  XrdClXRootDTransport.cc        XrdClXRootDTransport.hh
  XrdClInQueue.cc                XrdClInQueue.hh
  XrdClOutQueue.cc               XrdClOutQueue.hh
  XrdClTaskManager.cc            XrdClTaskManager.hh
  XrdClSIDManager.cc             XrdClSIDManager.hh
  XrdClFileSystem.cc             XrdClFileSystem.hh
  XrdClXRootDMsgHandler.cc       XrdClXRootDMsgHandler.hh
                                 XrdClBuffer.hh
                                 XrdClMessage.hh
  XrdClMessageUtils.cc           XrdClMessageUtils.hh
  XrdClXRootDResponses.cc        XrdClXRootDResponses.hh
                                 XrdClRequestSync.hh
  XrdClFile.cc                   XrdClFile.hh
  XrdClFileStateHandler.cc       XrdClFileStateHandler.hh
  XrdClCopyProcess.cc            XrdClCopyProcess.hh
  XrdClClassicCopyJob.cc         XrdClClassicCopyJob.hh
  XrdClThirdPartyCopyJob.cc      XrdClThirdPartyCopyJob.hh
  XrdClAsyncSocketHandler.cc     XrdClAsyncSocketHandler.hh
  XrdClChannelHandlerList.cc     XrdClChannelHandlerList.hh
  XrdClForkHandler.cc            XrdClForkHandler.hh
  XrdClCheckSumManager.cc        XrdClCheckSumManager.hh
  XrdClTransportManager.cc       XrdClTransportManager.hh
                                 XrdClSyncQueue.hh
  XrdClJobManager.cc             XrdClJobManager.hh
                                 XrdClResponseJob.hh
  XrdClFileTimer.cc              XrdClFileTimer.hh
                                 XrdClPlugInInterface.hh
  XrdClPlugInManager.cc          XrdClPlugInManager.hh
                                 XrdClPropertyList.hh
                                 XrdClCopyJob.hh
  XrdClFileSystemUtils.cc        XrdClFileSystemUtils.hh
  XrdClTPFallBackCopyJob.cc      XrdClTPFallBackCopyJob.hh
  XrdClMetalinkRedirector.cc     XrdClMetalinkRedirector.hh
  XrdClRedirectorRegistry.cc     XrdClRedirectorRegistry.hh
  XrdClXCpCtx.cc                 XrdClXCpCtx.hh
  XrdClXCpSrc.cc                 XrdClXCpSrc.hh
  XrdClLocalFileHandler.cc       XrdClLocalFileHandler.hh
  XrdClLocalFileTask.cc          XrdClLocalFileTask.hh
  XrdClZipListHandler.cc         XrdClZipListHandler.hh
  XrdClZipArchive.cc             XrdClZipArchive.hh
  XrdClOperations.cc             XrdClOperations.hh
  XrdClOperationHandlers.hh
  XrdClArg.hh
  XrdClFwd.hh
  XrdClParallelOperation.hh
  XrdClFileOperations.hh
  XrdClFileSystemOperations.hh
  XrdClZipOperations.hh
)

target_link_libraries(XrdCl
  PRIVATE
    XrdXml
    XrdUtils
    uuid::uuid
    ZLIB::ZLIB
    OpenSSL::SSL
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBS}
    ${CMAKE_DL_LIBS}
)

if(BUILD_XRDEC)
  target_sources(XrdCl
    PRIVATE
      ../XrdEc/XrdEcRedundancyProvider.cc
      ../XrdEc/XrdEcUtilities.cc
      ../XrdEc/XrdEcStrmWriter.cc
      ../XrdEc/XrdEcReader.cc
      XrdClEcHandler.cc
  )

  target_compile_definitions(XrdCl PRIVATE WITH_XRDEC)
  target_include_directories(XrdCl PRIVATE ${ISAL_INCLUDE_DIRS})
  target_link_libraries(XrdCl PRIVATE ${ISAL_LIBRARIES})
endif()

set_target_properties(XrdCl
  PROPERTIES VERSION 3.0.0 SOVERSION 3)

install(
  FILES
    XrdClAnyObject.hh
    XrdClBuffer.hh
    XrdClConstants.hh
    XrdClCopyProcess.hh
    XrdClDefaultEnv.hh
    XrdClEnv.hh
    XrdClFile.hh
    XrdClFileSystem.hh
    XrdClFileSystemUtils.hh
    XrdClMonitor.hh
    XrdClStatus.hh
    XrdClURL.hh
    XrdClXRootDResponses.hh
    XrdClOptional.hh
    XrdClPlugInInterface.hh
    XrdClPropertyList.hh
    XrdClLog.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdCl
)

install(
  FILES
    # Additional client headers
    XrdClJobManager.hh
    XrdClMessage.hh
    XrdClPlugInManager.hh
    XrdClPostMaster.hh
    XrdClPostMasterInterfaces.hh
    XrdClTransportManager.hh
    XrdClResponseJob.hh
    XrdClSyncQueue.hh
    XrdClZipArchive.hh
    XrdClZipCache.hh
    # Declarative operations
    XrdClOperations.hh
    XrdClOperationHandlers.hh
    XrdClOperationTimeout.hh
    XrdClArg.hh
    XrdClCtx.hh
    XrdClFwd.hh
    XrdClParallelOperation.hh
    XrdClFileOperations.hh
    XrdClFileSystemOperations.hh
    XrdClFinalOperation.hh
    XrdClUtils.hh
    XrdClXRootDTransport.hh
    XrdClZipOperations.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/private/XrdCl
)

install(
  TARGETS
    XrdCl
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PRIVATE_HEADER
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/private/XrdCl
)

if(NOT XRDCL_LIB_ONLY)
  add_executable(xrdcp XrdClCopy.cc)

  target_link_libraries(xrdcp
    XrdCl
    XrdUtils
    XrdAppUtils
  )

  add_executable(xrdfs XrdClFS.cc XrdClFSExecutor.cc XrdClFSExecutor.hh)

  target_link_libraries(xrdfs
    XrdCl
    XrdUtils
    ${READLINE_LIBRARY}
    ${NCURSES_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  if(READLINE_FOUND)
    target_include_directories(xrdfs PRIVATE ${READLINE_INCLUDE_DIR})
  endif()

  install(
    TARGETS
      xrdcp
      xrdfs
    RUNTIME
      DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(CODE "
    execute_process(COMMAND ln -sf xrdcp xrdcopy
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
    )
  ")
endif()
