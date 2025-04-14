add_executable(xrdpfc-unit-tests XrdPfcTests.cc)

target_link_libraries(xrdpfc-unit-tests GTest::GTest GTest::Main)
target_include_directories(xrdpfc-unit-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

gtest_discover_tests(xrdpfc-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
