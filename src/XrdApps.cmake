
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_APP_UTILS_VERSION   1.0.0 )
set( XRD_APP_UTILS_SOVERSION 1 )

#-------------------------------------------------------------------------------
# xrdadler32
#-------------------------------------------------------------------------------
add_executable(
  xrdadler32
  XrdApps/Xrdadler32.cc )

target_link_libraries(
  xrdadler32
  XrdPosix
  XrdClient
  XrdUtils
  pthread
  ${ZLIB_LIBRARY} )

#-------------------------------------------------------------------------------
# cconfig
#-------------------------------------------------------------------------------
add_executable(
  cconfig
  XrdApps/XrdAppsCconfig.cc )

target_link_libraries(
  cconfig
  XrdUtils )

#-------------------------------------------------------------------------------
# mpxstats
#-------------------------------------------------------------------------------
add_executable(
  mpxstats
  XrdApps/XrdMpxStats.cc )

target_link_libraries(
  mpxstats
  XrdUtils
  ${EXTRA_LIBS}
  pthread
  ${SOCKET_LIBRARY} )

#-------------------------------------------------------------------------------
# wait41
#-------------------------------------------------------------------------------
add_executable(
  wait41
  XrdApps/XrdWait41.cc )

target_link_libraries(
  wait41
  XrdUtils
  pthread
  ${EXTRA_LIBS} )

#-------------------------------------------------------------------------------
# xrdmapc
#-------------------------------------------------------------------------------
add_executable(
  xrdmapc
  XrdApps/XrdMapCluster.cc )

target_link_libraries(
  xrdmapc
  XrdCl
  XrdUtils )

#-------------------------------------------------------------------------------
# AppUtils
#-------------------------------------------------------------------------------
add_library(
  XrdAppUtils
  SHARED
  XrdApps/XrdCpConfig.cc          XrdApps/XrdCpConfig.hh
  XrdApps/XrdCpFile.cc            XrdApps/XrdCpFile.hh )

target_link_libraries(
  XrdAppUtils
  XrdUtils )

set_target_properties(
  XrdAppUtils
  PROPERTIES
  VERSION   ${XRD_APP_UTILS_VERSION}
  SOVERSION ${XRD_APP_UTILS_SOVERSION} )

#-------------------------------------------------------------------------------
# xrdCp
#-------------------------------------------------------------------------------
add_executable(
  xrdcp-old
  XrdApps/XrdCpy.cc
  XrdClient/XrdcpXtremeRead.cc         XrdClient/XrdcpXtremeRead.hh
  XrdClient/XrdCpMthrQueue.cc          XrdClient/XrdCpMthrQueue.hh
  XrdClient/XrdCpWorkLst.cc            XrdClient/XrdCpWorkLst.hh )

target_link_libraries(
  xrdcp-old
  XrdClient
  XrdUtils
  XrdAppUtils
  dl
  pthread
  ${EXTRA_LIBS} )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS xrdadler32 cconfig mpxstats wait41 xrdcp-old XrdAppUtils
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdadler32.1
  ${PROJECT_SOURCE_DIR}/docs/man/xrdcp-old.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/mpxstats.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 )
