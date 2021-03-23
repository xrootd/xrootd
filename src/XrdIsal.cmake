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

set( ISAL_BUILDDIR "${CMAKE_BINARY_DIR}/isal/build" CACHE INTERNAL "" )
set( ISAL_INCDIR   "${CMAKE_BINARY_DIR}/isal/include" CACHE INTERNAL "" )
set( ISAL_LIBDIR   "${CMAKE_BINARY_DIR}/isal/lib" CACHE INTERNAL "" )

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

ExternalProject_add(
        isa-l
        SOURCE_DIR          ${ISAL_BUILDDIR}
        BUILD_IN_SOURCE     1
        GIT_REPOSITORY      https://github.com/01org/isa-l.git
        GIT_TAG             ${ISAL_VERSION}
        CONFIGURE_COMMAND   ./autogen.sh COMMAND ./configure --with-pic
        BUILD_COMMAND       make ${MAKEOPTIONS}
        INSTALL_COMMAND     mkdir -p  ${ISAL_INCDIR}/isa-l
        COMMAND             mkdir -p  ${ISAL_LIBDIR}
        COMMAND             cp ${ISAL_HEADERS}                  ${ISAL_INCDIR}/isa-l
        COMMAND             cp ${ISAL_BUILDDIR}/isa-l.h         ${ISAL_INCDIR}/isa-l
        COMMAND             cp ${ISAL_BUILDDIR}/.libs/libisal.a ${ISAL_LIBDIR}/
)

