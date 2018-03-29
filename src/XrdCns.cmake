
include( XRootDCommon )

#-------------------------------------------------------------------------------
# The XrdCns library
#-------------------------------------------------------------------------------
add_library(
  XrdCnsLib
  STATIC
  XrdCns/XrdCnsLog.cc                XrdCns/XrdCnsLog.hh
  XrdCns/XrdCnsLogRec.cc             XrdCns/XrdCnsLogRec.hh

  XrdCns/XrdCnsXref.cc               XrdCns/XrdCnsXref.hh )

#-------------------------------------------------------------------------------
# XrdCns
#-------------------------------------------------------------------------------
add_executable(
  XrdCnsd
  XrdCns/XrdCnsConfig.cc             XrdCns/XrdCnsConfig.hh
  XrdCns/XrdCnsDaemon.cc             XrdCns/XrdCnsDaemon.hh
  XrdCns/XrdCnsInventory.cc          XrdCns/XrdCnsInventory.hh
  XrdCns/XrdCnsLogClient.cc          XrdCns/XrdCnsLogClient.hh
  XrdCns/XrdCnsLogFile.cc            XrdCns/XrdCnsLogFile.hh
  XrdCns/XrdCnsLogServer.cc          XrdCns/XrdCnsLogServer.hh 
  XrdCns/XrdCnsMain.cc )


target_link_libraries(
  XrdCnsd
  XrdCnsLib
  XrdClient
  XrdServer
  XrdUtils
  pthread
  ${EXTRA_LIBS} )

#-------------------------------------------------------------------------------
# cns_ssi
#-------------------------------------------------------------------------------
add_executable(
  cns_ssi
  XrdCns/XrdCnsSsi.cc            XrdCns/XrdCnsSsi.hh
  XrdCns/XrdCnsSsiCfg.cc         XrdCns/XrdCnsSsiCfg.hh
  XrdCns/XrdCnsSsiMain.cc
  XrdCns/XrdCnsSsiSay.hh )

target_link_libraries(
  cns_ssi
  XrdUtils
  XrdCnsLib
  pthread
  ${EXTRA_LIBS} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdCnsd cns_ssi
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/XrdCnsd.8
  ${PROJECT_SOURCE_DIR}/docs/man/cns_ssi.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 )

