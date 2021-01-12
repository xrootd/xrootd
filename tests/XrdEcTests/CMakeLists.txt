
include( XRootDCommon )
include_directories( ${CPPUNIT_INCLUDE_DIRS} ../common)

add_library(
  XrdEcTests MODULE
  StrmWriterTests.cc
  ReaderTests.cc
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
