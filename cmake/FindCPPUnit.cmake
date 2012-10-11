# Try to find CPPUnit
# Once done, this will define
#
# CPPUNIT_FOUND - system has cppunit
# CPPUNIT_INCLUDE_DIRS - the cppunit include directories
# CPPUNIT_LIBRARIES - cppunit libraries directories

find_path( CPPUNIT_INCLUDE_DIRS cppunit/ui/text/TestRunner.h
  HINTS
  ${CPPUNIT_DIR}
  $ENV{CPPUNIT_DIR}
  /usr
  /opt
  PATH_SUFFIXES include
)

find_library( CPPUNIT_LIBRARIES cppunit
  HINTS
  ${CPPUNIT_DIR}
  $ENV{CPPUNIT_DIR}
  /usr
  /opt
  PATH_SUFFIXES lib
)

set(CPPUNIT_INCLUDE_DIRS ${CPPUNIT_INCLUDE_DIR})
set(CPPUNIT_LIBRARIES ${CPPUNIT_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cppunit DEFAULT_MSG CPPUNIT_INCLUDE_DIRS CPPUNIT_LIBRARIES)
