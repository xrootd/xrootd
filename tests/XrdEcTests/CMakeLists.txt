
include( XRootDCommon )
include_directories( ${CPPUNIT_INCLUDE_DIRS} ../common )

link_directories( ${ISAL_LIBDIR} )
include_directories( ${ISAL_INCDIR} )

add_library(
  XrdEcTests MODULE
  MicroTest.cc
)

target_link_libraries(
  XrdEcTests
  XrdEc )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdEcTests
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
