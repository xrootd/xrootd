add_executable(xrdofs-prep-gpi-tests
  XrdOfsPrepGPITests.cc
  ${PROJECT_SOURCE_DIR}/src/XrdOfs/XrdOfsPrepGPI.cc
)

target_compile_definitions(xrdofs-prep-gpi-tests PRIVATE XRD_PREP_GPI_TESTING)

target_link_libraries(xrdofs-prep-gpi-tests
  PRIVATE
    XrdUtils
    XrdServer
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(xrdofs-prep-gpi-tests
  TEST_PREFIX XrdOfsPrepGPI::
  DISCOVERY_TIMEOUT 10
  PROPERTIES TIMEOUT 30
)

