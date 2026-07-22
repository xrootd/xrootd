add_executable(xrdossmirage-unit-tests XrdOssMirageTests.cc
        XrdOssMirageFileTests.cc
        XrdOssMirageXAttrTests.cc
        XrdOssMirageXAttrWrapperTests.cc
        XrdOssMirageWrapperTests.cc
        XrdOssMirageWrapperFileTests.cc
        XrdOssMirageWrapperDirTests.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirage.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageDir.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageFile.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageXAttr.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageXAttrWrapper.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageWrapper.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageWrapperFile.cc
        ${PROJECT_SOURCE_DIR}/src/XrdOssMirage/XrdOssMirageWrapperDir.cc
        )

target_link_libraries(xrdossmirage-unit-tests GTest::gmock GTest::gtest GTest::gtest_main XrdServer XrdUtils)

gtest_discover_tests(xrdossmirage-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
