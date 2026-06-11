add_executable(xrdossmirage-unit-tests XrdOssMirageTests.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirage.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageDir.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageFile.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageXAttr.cc
        )

target_link_libraries(xrdossmirage-unit-tests GTest::gtest GTest::gtest_main XrdServer XrdUtils)

gtest_discover_tests(xrdossmirage-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
