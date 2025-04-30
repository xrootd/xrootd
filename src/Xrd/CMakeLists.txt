target_sources(XrdUtils
  PRIVATE
    XrdBuffer.cc     XrdBuffer.hh
    XrdBuffXL.cc     XrdBuffXL.hh
    XrdInet.cc       XrdInet.hh
    XrdInfo.cc       XrdInfo.hh
    XrdJob.hh
    XrdLink.cc       XrdLink.hh
    XrdLinkCtl.cc    XrdLinkCtl.hh
                     XrdLinkInfo.hh
    XrdLinkXeq.cc    XrdLinkXeq.hh
    XrdLinkMatch.cc  XrdLinkMatch.hh
    XrdGlobals.cc
    XrdMonitor.cc    XrdMonRoll.hh
    XrdMonRoll.cc    XrdMonRoll.hh
    XrdObject.icc    XrdObject.hh
    XrdPoll.cc       XrdPoll.hh
                     XrdPollE.hh
                     XrdPollE.icc
                     XrdPollInfo.hh
                     XrdPollPoll.hh
                     XrdPollPoll.icc
                     XrdProtocol.hh
    XrdScheduler.cc  XrdScheduler.hh
    XrdSendQ.cc      XrdSendQ.hh
                     XrdTrace.hh
)

if(XRDCL_ONLY)
  return()
endif()

add_executable(xrootd
  XrdConfig.cc    XrdConfig.hh
  XrdProtLoad.cc  XrdProtLoad.hh
  XrdStats.cc     XrdStats.hh
  XrdMain.cc
)

target_link_libraries(xrootd
  XrdServer
  XrdUtils
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${EXTRA_LIBS}
  ${SOCKET_LIBRARY}
)

install(TARGETS xrootd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
