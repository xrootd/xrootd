# Create a static library for testing (since MODULE libraries can't be linked)
add_library(XrdThrottleTestLib STATIC
  ${PROJECT_SOURCE_DIR}/src/XrdThrottle/XrdThrottleManager.cc
  ${PROJECT_SOURCE_DIR}/src/XrdThrottle/XrdThrottleConfig.cc
)

target_link_libraries(XrdThrottleTestLib
  PRIVATE
    XrdUtils
    XrdServer
)

target_include_directories(XrdThrottleTestLib
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/XrdSciTokens/vendor/inih
)

# Create the test executable
add_executable(xrdthrottle-unit-tests XrdThrottleUserLimitsTests.cc)

target_link_libraries(xrdthrottle-unit-tests
  PRIVATE
    XrdThrottleTestLib
    XrdUtils
    XrdServer
    GTest::GTest
    GTest::Main
)

target_include_directories(xrdthrottle-unit-tests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/XrdSciTokens/vendor/inih
)

gtest_discover_tests(xrdthrottle-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)

