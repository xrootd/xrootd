
include( XRootDCommon )

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
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS xrdadler32 cconfig mpxstats wait41
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/xrdadler32.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )

install(
  FILES
  ${PROJECT_SOURCE_DIR}/docs/man/mpxstats.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 )
