
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_CL_VERSION   3.0.0 )
set( XRD_CL_SOVERSION 3 )

#-------------------------------------------------------------------------------
# The XrdCl Pipelining API
# (we build this API only if the compiler has a good support for c++11)
#-------------------------------------------------------------------------------
if( XrdClPipelines )
  set( XrdClPipelineSources
       XrdClOperations.cc            XrdClOperations.hh
       XrdClOperationHandlers.hh
       XrdClArg.hh
       XrdClFwd.hh
       XrdClParallelOperation.hh
       XrdClFileOperations.hh
       XrdClFileSystemOperations.hh
       XrdClZipOperations.hh
  )
endif()

#-------------------------------------------------------------------------------
# XrdEc sources
#-------------------------------------------------------------------------------
if( BUILD_XRDEC )
  link_directories( ${ISAL_LIBDIR} )
  include_directories( ${ISAL_INCDIR} )
  set( XrdEcSources
       ${CMAKE_SOURCE_DIR}/src/XrdEc/XrdEcRedundancyProvider.cc
       ${CMAKE_SOURCE_DIR}/src/XrdEc/XrdEcUtilities.cc
       ${CMAKE_SOURCE_DIR}/src/XrdEc/XrdEcStrmWriter.cc
       ${CMAKE_SOURCE_DIR}/src/XrdEc/XrdEcReader.cc
  )
  set( ISAL_LIB isal )
  add_compile_definitions( WITH_XRDEC )
endif()

#-------------------------------------------------------------------------------
# The XrdCl lib
#-------------------------------------------------------------------------------
add_library(
  XrdCl
  SHARED
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
  XrdClZipArchive.cc            XrdClZipArchive.hh
  
  ${XrdClPipelineSources}

  ${XrdEcSources}
)

target_link_libraries(
  XrdCl
  XrdXml
  XrdUtils
  ${CMAKE_THREAD_LIBS_INIT}
  ${UUID_LIBRARY}
  ${ZLIB_LIBRARIES}
  ${EXTRA_LIBS}
  ${CMAKE_DL_LIBS}
  ${OPENSSL_LIBRARIES}
  ${ISAL_LIB})

set_target_properties(
  XrdCl
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES ""
  VERSION   ${XRD_CL_VERSION}
  SOVERSION ${XRD_CL_SOVERSION} )

if( BUILD_XRDEC )
  add_dependencies( XrdCl isa-l )
endif()

#-------------------------------------------------------------------------------
# xrdfs
#-------------------------------------------------------------------------------
if( NOT XRDCL_LIB_ONLY )
add_executable(
  xrdfs
  XrdClFS.cc
  XrdClFSExecutor.cc         XrdClFSExecutor.hh )

target_link_libraries(
  xrdfs
  ${CMAKE_THREAD_LIBS_INIT}
  XrdCl
  XrdUtils
  ${READLINE_LIBRARY}
  ${NCURSES_LIBRARY} )
endif()

#-------------------------------------------------------------------------------
# xrdcopy
#-------------------------------------------------------------------------------
if( NOT XRDCL_LIB_ONLY )
add_executable(
  xrdcp
  XrdClCopy.cc )

target_link_libraries(
  xrdcp
  XrdCl
  XrdAppUtils )
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCl
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

if( NOT XRDCL_LIB_ONLY )
install(
  TARGETS xrdfs xrdcp
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
endif()

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
    XrdClPlugInManager.hh
    XrdClPropertyList.hh
    XrdClLog.hh
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdCl )
  
install(
  FILES
    # Additional client headers
    XrdClMessage.hh
    XrdClPostMaster.hh
    XrdClPostMasterInterfaces.hh
    XrdClTransportManager.hh
    XrdClResponseJob.hh
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
    XrdClZipOperations.hh 
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/private/XrdCl )

if( NOT XRDCL_LIB_ONLY )
install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdfs.1
  ${PROJECT_SOURCE_DIR}/docs/man/xrdcp.1
  ${PROJECT_SOURCE_DIR}/docs/man/xrdmapc.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf xrdcp xrdcopy
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR} )"
)

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf xrdcp.1 xrdcopy.1
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1 )"
)

install(
  CODE "
    FOREACH(MANPAGE xrdfs.1 xrdcp.1 xrdmapc.1)
      MESSAGE( \"-- Processing: \" \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1/\${MANPAGE} )
      EXECUTE_PROCESS(
        COMMAND cat \${MANPAGE}
        COMMAND sed  -e \"s/__VERSION__/${XROOTD_VERSION}/\"
        OUTPUT_FILE \${MANPAGE}.new
        WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1 )
      EXECUTE_PROCESS(
        COMMAND mv -f \${MANPAGE}.new \${MANPAGE}
        WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1 )
    ENDFOREACH()"
)

endif()
