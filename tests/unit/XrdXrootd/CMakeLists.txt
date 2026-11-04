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

gtest_discover_tests(xrdxrootd-redir-helper-tests TEST_PREFIX unit::XrdXrootd::
    PROPERTIES DISCOVERY_TIMEOUT 10)

# The monitor statics are global to the process and Init() runs only once, so
# these tests have their own executable. The test file provides main().
add_executable(xrdxrootd-monfile-unit-tests XrdXrootdMonFileTests.cc)

target_link_libraries(xrdxrootd-monfile-unit-tests
    XrdServer
    XrdUtils
    GTest::gtest)

gtest_discover_tests(xrdxrootd-monfile-unit-tests TEST_PREFIX unit::XrdXrootd::
    PROPERTIES DISCOVERY_TIMEOUT 10)

# The transfer map tests need the "xfr" option and the largest buffers, which
# are global to the process, so they have their own executable too.
add_executable(xrdxrootd-monxfr-unit-tests XrdXrootdMonXfrTests.cc)

target_link_libraries(xrdxrootd-monxfr-unit-tests
    XrdServer
    XrdUtils
    GTest::gtest)

gtest_discover_tests(xrdxrootd-monxfr-unit-tests TEST_PREFIX unit::XrdXrootd::
    PROPERTIES DISCOVERY_TIMEOUT 10)

# The tests of the f-stream error records need the "err" option, which is
# global to the process, so they have their own executable too.
add_executable(xrdxrootd-monfile-err-unit-tests XrdXrootdMonFileErr.cc)

target_link_libraries(xrdxrootd-monfile-err-unit-tests
    XrdServer
    XrdUtils
    GTest::gtest)

gtest_discover_tests(xrdxrootd-monfile-err-unit-tests TEST_PREFIX unit::XrdXrootd::
    PROPERTIES DISCOVERY_TIMEOUT 10)

# The same tests without the "lfn" option. The suite has another name, so
# that the names of the tests differ from the names of the tests above.
add_executable(xrdxrootd-monfile-err-nolfn-unit-tests XrdXrootdMonFileErr.cc)

target_compile_definitions(xrdxrootd-monfile-err-nolfn-unit-tests
    PRIVATE MONFILEERR_NO_LFN)

target_link_libraries(xrdxrootd-monfile-err-nolfn-unit-tests
    XrdServer
    XrdUtils
    GTest::gtest)

gtest_discover_tests(xrdxrootd-monfile-err-nolfn-unit-tests TEST_PREFIX unit::XrdXrootd::
    PROPERTIES DISCOVERY_TIMEOUT 10)

# The tests which do not start the monitor
add_executable(xrdxrootd-unit-tests XrdXrootdFileStats.cc)

target_link_libraries(xrdxrootd-unit-tests
    XrdServer
    XrdUtils
    GTest::gtest
    GTest::gtest_main)

gtest_discover_tests(xrdxrootd-unit-tests TEST_PREFIX unit::XrdXrootd::
    PROPERTIES DISCOVERY_TIMEOUT 10)
