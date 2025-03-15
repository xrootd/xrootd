#-------------------------------------------------------------------------------
# The XRootD protocol implementation
#-------------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdXrootdAdmin.cc      XrdXrootdAdmin.hh
    XrdXrootdAioBuff.cc    XrdXrootdAioBuff.hh
    XrdXrootdAioFob.cc     XrdXrootdAioFob.hh
    XrdXrootdAioPgrw.cc    XrdXrootdAioPgrw.hh
    XrdXrootdAioTask.cc    XrdXrootdAioTask.hh
    XrdXrootdBridge.cc     XrdXrootdBridge.hh
    XrdXrootdCallBack.cc   XrdXrootdCallBack.hh
    XrdXrootdConfig.cc
    XrdXrootdConfigMon.cc
    XrdXrootdFile.cc       XrdXrootdFile.hh
                           XrdXrootdFileLock.hh
    XrdXrootdFileLock1.cc  XrdXrootdFileLock1.hh
                           XrdXrootdFileStats.hh
    XrdXrootdGSReal.cc     XrdXrootdGSReal.hh
    XrdXrootdGStream.cc    XrdXrootdGStream.hh
    XrdXrootdJob.cc        XrdXrootdJob.hh
    XrdXrootdLoadLib.cc
                           XrdXrootdMonData.hh
    XrdXrootdMonFMap.cc    XrdXrootdMonFMap.hh
    XrdXrootdMonFile.cc    XrdXrootdMonFile.hh
    XrdXrootdMonitor.cc    XrdXrootdMonitor.hh
    XrdXrootdNormAio.cc    XrdXrootdNormAio.hh
    XrdXrootdPgrwAio.cc    XrdXrootdPgrwAio.hh
    XrdXrootdPgwBadCS.cc   XrdXrootdPgwBadCS.hh
    XrdXrootdPgwCtl.cc     XrdXrootdPgwCtl.hh
    XrdXrootdPgwFob.cc     XrdXrootdPgwFob.hh
    XrdXrootdPio.cc        XrdXrootdPio.hh
    XrdXrootdPrepare.cc    XrdXrootdPrepare.hh
    XrdXrootdProtocol.cc   XrdXrootdProtocol.hh
                           XrdXrootdReqID.hh
    XrdXrootdResponse.cc   XrdXrootdResponse.hh
    XrdXrootdStats.cc      XrdXrootdStats.hh
    XrdXrootdTpcMon.cc     XrdXrootdTpcMon.hh
                           XrdXrootdTrace.hh
    XrdXrootdTransPend.cc  XrdXrootdTransPend.hh
    XrdXrootdTransSend.cc  XrdXrootdTransSend.hh
    XrdXrootdTransit.cc    XrdXrootdTransit.hh
                           XrdXrootdWVInfo.hh
                           XrdXrootdXPath.hh
    XrdXrootdXeq.cc        XrdXrootdXeq.hh
    XrdXrootdXeqChkPnt.cc
    XrdXrootdXeqFAttr.cc
    XrdXrootdXeqPgrw.cc
)

set(XrdXrootd XrdXrootd-${PLUGIN_VERSION})

add_library(${XrdXrootd} MODULE XrdXrootdPlugin.cc)
target_link_libraries(${XrdXrootd} PRIVATE XrdServer XrdUtils ${EXTRA_LIBS})

install(TARGETS ${XrdXrootd} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
