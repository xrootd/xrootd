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
  if(BUILD_HTTP AND ENABLE_SERVER_TESTS)
    # Synthetic credentials for a loopback-only integration fixture. Never installed.
    add_library(XrdOfsPrepTestHttpHandler MODULE TestHttpHandler.cc)
    target_link_libraries(XrdOfsPrepTestHttpHandler PRIVATE XrdHttpUtils)
    add_library(XrdOfsPrepTestAuth MODULE TestAuth.cc)
    target_link_libraries(XrdOfsPrepTestAuth PRIVATE XrdUtils)
    add_test(NAME XrdOfsPrep::Integration
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/integration.py"
        "${PROJECT_BINARY_DIR}" "$<TARGET_FILE:XrdOfsPrepTestAuth>")
    set_tests_properties(XrdOfsPrep::Integration PROPERTIES TIMEOUT 120)
  endif()
endif()
