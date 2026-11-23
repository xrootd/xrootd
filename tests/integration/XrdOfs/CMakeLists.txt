if(NOT TARGET XrdServer)
  return()
endif()

add_executable(xrdofs-integration-tests
  XrdOfsPrepGPI.cc
  ${PROJECT_SOURCE_DIR}/src/XrdOfs/XrdOfsPrepGPI.cc
)
target_compile_definitions(xrdofs-integration-tests PRIVATE XRD_PREP_GPI_TESTING)
target_link_libraries(xrdofs-integration-tests PRIVATE XrdUtils XrdServer GTest::gtest_main)

gtest_discover_tests(xrdofs-integration-tests TEST_PREFIX integration.XrdOfs.
  DISCOVERY_TIMEOUT 10 NO_PRETTY_VALUES PROPERTIES TIMEOUT 30)
