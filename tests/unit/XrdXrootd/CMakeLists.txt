# XrdXrootd unit tests. The tested classes are compiled into the XrdServer
# shared library, so the tests are only built when XrdServer is being built
# (i.e. not in client-only configurations).
if(NOT TARGET XrdServer)
    return()
endif()

add_executable(xrdxrootd-redir-helper-tests XrdXrootdRedirHelperTests.cc)

target_link_libraries(xrdxrootd-redir-helper-tests
    XrdServer
    XrdUtils
    GTest::gtest
    GTest::gtest_main)

gtest_discover_tests(xrdxrootd-redir-helper-tests
    PROPERTIES DISCOVERY_TIMEOUT 10)

# The monitor statics are global to the process and Init() runs only once, so
# these tests have their own executable. The test file provides main().
add_executable(xrdxrootd-monfile-unit-tests XrdXrootdMonFileTests.cc)

target_link_libraries(xrdxrootd-monfile-unit-tests
    XrdServer
    XrdUtils
    GTest::gtest)

gtest_discover_tests(xrdxrootd-monfile-unit-tests
    PROPERTIES DISCOVERY_TIMEOUT 10)

# The transfer map tests need the "xfr" option and the largest buffers, which
# are global to the process, so they have their own executable too.
add_executable(xrdxrootd-monxfr-unit-tests XrdXrootdMonXfrTests.cc)

target_link_libraries(xrdxrootd-monxfr-unit-tests
    XrdServer
    XrdUtils
    GTest::gtest)

gtest_discover_tests(xrdxrootd-monxfr-unit-tests
    PROPERTIES DISCOVERY_TIMEOUT 10)
