if(NOT BUILD_XRDEC)
  return()
endif()

add_executable(xrdec-unit-tests XrdEcTests.cc)

target_link_libraries(xrdec-unit-tests
  XrdEc XrdTestUtils GTest::GTest GTest::Main ${ISAL_LIBRARIES})

target_include_directories(xrdec-unit-tests PRIVATE ${ISAL_INCLUDE_DIRS})

gtest_discover_tests(xrdec-unit-tests TEST_PREFIX XrdEc::)
