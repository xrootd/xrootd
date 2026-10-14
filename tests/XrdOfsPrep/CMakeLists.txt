if(NOT TARGET XrdOfsPrepPersistCore)
  return()
endif()

add_executable(xrd-ofs-prep-tests XrdOfsPrepTests.cc
  ${PROJECT_SOURCE_DIR}/src/XrdOfs/XrdOfsPrepPersistEntry.cc)
target_link_libraries(xrd-ofs-prep-tests PRIVATE XrdOfsPrepPersistCore GTest::gtest_main)
gtest_discover_tests(xrd-ofs-prep-tests TEST_PREFIX XrdOfsPrep:: DISCOVERY_TIMEOUT 30)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME XrdOfsPrep::Mock
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_mock_tape.py")
endif()
