# The tested code links XrdServer, which a client-only build does not have
if(NOT TARGET XrdServer)
  return()
endif()

add_executable(xrdossmirage-unit-tests XrdOssMirageTests.cc
        XrdOssMirageFileTests.cc
        XrdOssMirageXAttrTests.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirage.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageDir.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageFile.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageXAttr.cc
        )

target_link_libraries(xrdossmirage-unit-tests GTest::gtest GTest::gtest_main XrdServer XrdUtils)

gtest_discover_tests(xrdossmirage-unit-tests TEST_PREFIX unit::XrdOssMirage::
  PROPERTIES DISCOVERY_TIMEOUT 10)
