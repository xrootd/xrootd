if ( XRDCL_ONLY )
  return()
endif()

add_executable(
  xrdshmap
  XrdShMap.cc
)

target_link_libraries(
  xrdshmap
  ZLIB::ZLIB
  XrdSsiShMap )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS xrdshmap
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
