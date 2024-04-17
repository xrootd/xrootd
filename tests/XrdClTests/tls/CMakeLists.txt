

#-------------------------------------------------------------------------------
# xrdcopy
#-------------------------------------------------------------------------------
if( NOT XRDCL_ONLY )

add_executable(
  xrdcl-tls
  xrdcl-tls.cc )
  
target_link_libraries(
  xrdcl-tls
  XrdCl
  XrdPosix
  XrdXml
  XrdAppUtils
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT} )
  
  
add_executable(
  xrdsrv-tls
  xrdsrv-tls.cc )
  
target_link_libraries(
  xrdsrv-tls
  XrdUtils
  OpenSSL::SSL
  ${CMAKE_THREAD_LIBS_INIT} )
  
endif()
