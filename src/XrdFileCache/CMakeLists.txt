
cmake_minimum_required( VERSION 2.6 )
project( xrootd-file-cache )

set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

find_package( Xrootd REQUIRED )

if( CMAKE_COMPILER_IS_GNUCXX )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-sign-compare -Werr" )
endif()

if( CMAKE_COMPILER_IS_GNUCC )
  set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-sign-compare " )
endif()

INCLUDE (CheckCXXSourceCompiles)
SET(CMAKE_REQUIRED_INCLUDES ${XROOTD_INCLUDES} ${XROOTD_INCLUDES_PRIVATE} )
check_cxx_source_compiles("
#include <XrdOuc/XrdOucIOVec.hh>
int main() {
    XrdOucIOVec readV;
    return 0;
}
" HAVE_READV)

if(HAVE_READV)
  add_definitions(-DHAVE_READV)
endif()

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
  SET(CMAKE_INSTALL_LIBDIR "lib")
endif()

include_directories ("${PROJECT_SOURCE_DIR}")
add_subdirectory( src )
add_subdirectory( test/xrdfragcp )

