#-------------------------------------------------------------------------------
# The Open File System and its dependencies
#-------------------------------------------------------------------------------

target_sources(XrdServer
  PRIVATE
    XrdOfs.cc          XrdOfs.hh
    XrdOfsChkPnt.cc    XrdOfsChkPnt.hh
    XrdOfsCksFile.cc   XrdOfsCksFile.hh
    XrdOfsConfig.cc
    XrdOfsConfigCP.cc  XrdOfsConfigCP.hh
    XrdOfsConfigPI.cc  XrdOfsConfigPI.hh
    XrdOfsCPFile.cc    XrdOfsCPFile.hh
    XrdOfsEvr.cc       XrdOfsEvr.hh
    XrdOfsEvs.cc       XrdOfsEvs.hh
    XrdOfsFAttr.cc
    XrdOfsFS.cc
    XrdOfsFSctl.cc
                       XrdOfsFSctl_PI.hh
    XrdOfsHandle.cc    XrdOfsHandle.hh
    XrdOfsPoscq.cc     XrdOfsPoscq.hh
                       XrdOfsSecurity.hh
    XrdOfsStats.cc     XrdOfsStats.hh
    XrdOfsTPC.cc       XrdOfsTPC.hh
    XrdOfsTPCAuth.cc   XrdOfsTPCAuth.hh
                       XrdOfsTPCConfig.hh
    XrdOfsTPCJob.cc    XrdOfsTPCJob.hh
    XrdOfsTPCInfo.cc   XrdOfsTPCInfo.hh
    XrdOfsTPCProg.cc   XrdOfsTPCProg.hh
                       XrdOfsTrace.hh
)

#-------------------------------------------------------------------------------
# Ofs Generic Prepare plugin library
#-------------------------------------------------------------------------------
set(XrdOfsPrepGPI XrdOfsPrepGPI-${PLUGIN_VERSION})
add_library(${XrdOfsPrepGPI} MODULE XrdOfsPrepGPI.cc)
target_link_libraries(${XrdOfsPrepGPI} PRIVATE XrdUtils)

install(TARGETS ${XrdOfsPrepGPI} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Optional protocol-neutral, durable coordinator around an XrdOfsPrepare backend.
add_library(XrdOfsPrepPersistCore STATIC XrdOfsPrepStore.cc XrdOfsPrepPersist.cc)
set_target_properties(XrdOfsPrepPersistCore PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(XrdOfsPrepPersistCore PUBLIC XrdUtils Threads::Threads uuid::uuid)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(XrdOfsPrepPersistCore PUBLIC stdc++fs)
endif()
add_library(XrdOfsPrepPersist-${PLUGIN_VERSION} MODULE XrdOfsPrepPersistEntry.cc)
target_link_libraries(XrdOfsPrepPersist-${PLUGIN_VERSION} PRIVATE XrdOfsPrepPersistCore)
install(TARGETS XrdOfsPrepPersist-${PLUGIN_VERSION} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
