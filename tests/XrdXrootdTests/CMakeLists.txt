# XrdXrootd helper-class unit tests.  XrdXrootdRedirHelper is compiled into
# the XrdServer shared library, so the tests are only built when XrdServer is
# being built (i.e. not in client-only configurations).
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

# f-stream monitoring buffer tests. These stand up the process-wide monitor
# against a loopback UDP socket, which can only be done once per process, so
# they need their own executable. The test provides its own main().
add_executable(xrdxrootd-monfile-tests XrdXrootdMonFileTests.cc)

target_link_libraries(xrdxrootd-monfile-tests
    XrdServer
    XrdUtils
    GTest::gtest)

gtest_discover_tests(xrdxrootd-monfile-tests
    PROPERTIES DISCOVERY_TIMEOUT 10)
