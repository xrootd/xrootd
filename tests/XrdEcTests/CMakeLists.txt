
add_library(
  XrdEcTests MODULE
  MicroTest.cc
)

target_link_libraries(
  XrdEcTests
  PRIVATE
  XrdEc
  XrdCl
  XrdUtils
  ${ISAL_LIBRARIES}
  ${CPPUNIT_LIBRARIES})

target_include_directories(XrdEcTests PRIVATE ../common ${CPPUNIT_INCLUDE_DIRS} ${ISAL_INCLUDE_DIRS})

foreach(TEST
  AlignedWriteTest
  SmallWriteTest
  BigWriteTest
  VectorReadTest
  IllegalVectorReadTest
  AlignedWrite1MissingTest
  AlignedWrite2MissingTest
  AlignedWriteTestIsalCrcNoMt
  SmallWriteTestIsalCrcNoMt
  BigWriteTestIsalCrcNoMt
  AlignedWrite1MissingTestIsalCrcNoMt
  AlignedWrite2MissingTestIsalCrcNoMt)
    add_test(NAME XrdEc::${TEST}
      COMMAND $<TARGET_FILE:test-runner> $<TARGET_FILE:XrdEcTests>
        "All Tests/MicroTest/MicroTest::${TEST}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdEcTests
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
