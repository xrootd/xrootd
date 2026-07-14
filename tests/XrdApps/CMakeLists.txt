add_executable(xrd-cli-unit-tests
  XrdCliCommandLineTest.cc
  ${PROJECT_SOURCE_DIR}/src/XrdApps/XrdCliCommandLine.cc
)

set_target_properties(xrd-cli-unit-tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED TRUE
)

target_link_libraries(xrd-cli-unit-tests
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(xrd-cli-unit-tests
  TEST_PREFIX XrdApps::xrd-cli::
  PROPERTIES DISCOVERY_TIMEOUT 10
)

if(TARGET xrd)
  set(XRD_CLI_DELEGATION_ENV
    "BATS_LIB_PATH=${PROJECT_SOURCE_DIR}/vendor/bats/lib/bats"
    "XRD=$<TARGET_FILE:xrd>"
  )

  add_test(NAME XrdApps::xrd-cli-delegation
    COMMAND ${PROJECT_SOURCE_DIR}/vendor/bats/bin/bats
            ${CMAKE_CURRENT_SOURCE_DIR}/xrd-cli-delegation.bats)

  set_tests_properties(XrdApps::xrd-cli-delegation PROPERTIES
    ENVIRONMENT "${XRD_CLI_DELEGATION_ENV}")
endif()
