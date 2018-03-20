
include( XRootDCommon )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )

#-------------------------------------------------------------------------------
# xrdcopy
#-------------------------------------------------------------------------------
add_executable(
  xrd_api_test
  xrd_api_test.cc )
  
target_link_libraries(
  xrd_api_test
  XrdCl
  XrdPosix
  XrdXml
  XrdAppUtils
  dl
  pthread )
  
  
add_executable(
  xrdsrv
  xrdsrv.cc )
  
target_link_libraries(
  xrdsrv
  ssl
  crypto )
  

