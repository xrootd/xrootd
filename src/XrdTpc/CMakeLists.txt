
cmake_minimum_required( VERSION 2.8 )
project( xrootd-tpc )

set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

find_package( Xrootd REQUIRED )

macro(use_cxx11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif ()
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()
endmacro(use_cxx11)
use_cxx11()

# Chunked responses were introduced after Xrootd 4.8.0.  Check to see if the symbol exists.
#include (CheckCXXSymbolExists)
SET( CMAKE_REQUIRED_INCLUDES "${XROOTD_PRIVATE_INCLUDES}" )
SET( CMAKE_REQUIRED_LIBRARIES "${XROOTD_HTTP_LIB}" )
SET( CMAKE_REQUIRED_FLAGS "" )
include (CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES("#include <XrdHttp/XrdHttpExtHandler.hh>

int main(int argc, char** argv)
{
  (void)argv;
  return ((int*)reinterpret_cast<void*>(&XrdHttpExtReq::ChunkResp))[argc];
}
" XRD_CHUNK_RESP)

if( CMAKE_COMPILER_IS_GNUCXX )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror" )
endif()
SET( CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
SET( CMAKE_MODULE_LINKER_FLAGS "-Wl,--no-undefined")

include (FindPkgConfig)
pkg_check_modules(CURL REQUIRED libcurl)

include_directories(${XROOTD_INCLUDES} ${XROOTD_PRIVATE_INCLUDES} ${CURL_INCLUDE_DIRS})

add_library(XrdHttpTPC SHARED src/tpc.cpp src/state.cpp src/configure.cpp src/stream.cpp src/multistream.cpp)
if ( XRD_CHUNK_RESP )
  set_target_properties(XrdHttpTPC PROPERTIES COMPILE_DEFINITIONS "XRD_CHUNK_RESP" )
endif ()

target_link_libraries(XrdHttpTPC -ldl ${XROOTD_UTILS_LIB} ${XROOTD_SERVER_LIB} ${XROOTD_HTTP_LIB} ${CURL_LIBRARIES})
set_target_properties(XrdHttpTPC PROPERTIES OUTPUT_NAME "XrdHttpTPC-4" SUFFIX ".so" LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/configs/export-lib-symbols")

SET(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Install path for libraries")

install(
  TARGETS XrdHttpTPC
  LIBRARY DESTINATION ${LIB_INSTALL_DIR}
)
