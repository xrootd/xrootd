
add_library(
  XrdEcTests MODULE
  MicroTest.cc
)

target_link_libraries(
  XrdEcTests
  PRIVATE
  XrdEc )

target_link_libraries(XrdEcTests PRIVATE ${ISAL_LIBRARIES})
target_include_directories(XrdEcTests PRIVATE ../common ${CPPUNIT_UNCLUDE_DIRS} ${ISAL_INCLUDE_DIRS})

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdEcTests
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
