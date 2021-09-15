
include( XRootDCommon )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )

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
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} )
  
endif()
