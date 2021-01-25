include( XRootDCommon )
include( ExternalProject )

#-------------------------------------------------------------------------------
# Build isa-l
#-------------------------------------------------------------------------------

set(MAKEOPTIONS "")
if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i386" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
    set(MAKEOPTIONS "arch=32")
endif()

#EXECUTE_PROCESS(
#     COMMAND git ls-remote --tags https://github.com/01org/isa-l
#     COMMAND awk "{print $2}"
#     COMMAND grep -v {}
#     COMMAND awk -F "/" "{print $3}"
#     COMMAND tail -1 
#     OUTPUT_VARIABLE ISAL_VERSION
#)

set( ISAL_VERSION v2.30.0 )
MESSAGE( STATUS "Building ISAL: ${ISAL_VERSION}" )

set( ISAL_BUILDDIR "${CMAKE_BINARY_DIR}/isal/build" )
set( ISAL_INCDIR   "${CMAKE_BINARY_DIR}/isal/include" )
set( ISAL_LIBDIR   "${CMAKE_BINARY_DIR}/isal/lib" )

set( ISAL_HEADERS 
	 ${ISAL_BUILDDIR}/include/crc64.h  
	 ${ISAL_BUILDDIR}/include/crc.h  
	 ${ISAL_BUILDDIR}/include/erasure_code.h  
	 ${ISAL_BUILDDIR}/include/gf_vect_mul.h  
	 ${ISAL_BUILDDIR}/include/igzip_lib.h  
	 ${ISAL_BUILDDIR}/include/mem_routines.h  
	 ${ISAL_BUILDDIR}/include/multibinary.asm  
	 ${ISAL_BUILDDIR}/include/raid.h  
	 ${ISAL_BUILDDIR}/include/reg_sizes.asm  
	 ${ISAL_BUILDDIR}/include/test.h  
	 ${ISAL_BUILDDIR}/include/types.h
)

set( ISAL_LIBS 
     ${ISAL_BUILDDIR}/.libs/libisal.so 
     ${ISAL_BUILDDIR}/.libs/libisal.so.2 
     ${ISAL_BUILDDIR}/.libs/libisal.so.2.0.30
)

ExternalProject_add(
        isa-l
        SOURCE_DIR          ${ISAL_BUILDDIR}
        BUILD_IN_SOURCE     1
        GIT_REPOSITORY      https://github.com/01org/isa-l.git
        GIT_TAG             ${ISAL_VERSION}
        CONFIGURE_COMMAND   ./autogen.sh COMMAND ./configure
        BUILD_COMMAND       make ${MAKEOPTIONS}
        INSTALL_COMMAND     mkdir -p  ${ISAL_INCDIR}/isa-l
        COMMAND             mkdir -p  ${ISAL_LIBDIR}
        COMMAND             cp ${ISAL_HEADERS}           ${ISAL_INCDIR}/isa-l
        COMMAND             cp ${ISAL_BUILDDIR}/isa-l.h  ${ISAL_INCDIR}/isa-l
        COMMAND             cp ${ISAL_LIBS}              ${ISAL_LIBDIR}
)

link_directories( ${ISAL_LIBDIR} )
include_directories( ${ISAL_INCDIR} )

#-------------------------------------------------------------------------------
# The XrdEc shared library
#-------------------------------------------------------------------------------

set( XRD_EC_VERSION   1.0.0 )
set( XRD_EC_SOVERSION 1 )

add_library(
  XrdEc
  SHARED
  XrdEcConfig.hh
  XrdEcObjCfg.hh
  XrdEcRedundancyProvider.hh   XrdEcRedundancyProvider.cc 
  XrdEcUtilities.hh            XrdEcUtilities.cc
  XrdEcWrtBuff.hh
  XrdEcThreadPool.hh
  XrdEcStrmWriter.hh           XrdEcStrmWriter.cc
  XrdEcReader.hh               XrdEcReader.cc
)

target_link_libraries(
  XrdEc
  XrdCl
  isal
)

set_target_properties(
  XrdEc
  PROPERTIES
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES ""
  VERSION   ${XRD_EC_VERSION}
  SOVERSION ${XRD_EC_SOVERSION} )
  
add_dependencies( XrdEc isa-l )

#------------------------------------------------------------------------------
# Install private header files
#------------------------------------------------------------------------------
install(
  FILES
    XrdEcReader.hh
    XrdEcObjCfg.hh
    XrdEcStrmWriter.hh
    XrdEcWrtBuff.hh
    XrdEcThreadPool.hh
    XrdEcUtilities.hh
    XrdEcObjCfg.hh
    XrdEcConfig.hh
    XrdEcRedundancyProvider.hh
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/private/XrdEc )
