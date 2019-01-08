
include( XRootDCommon )

add_executable(
  xrdshmap
  XrdShMap.cc
)

target_link_libraries(
  xrdshmap
  ${ZLIB_LIBRARIES}
  XrdSsiShMap )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS xrdshmap
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
