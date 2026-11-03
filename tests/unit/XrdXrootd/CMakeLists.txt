# XrdXrootd unit tests. The tested classes are compiled into the XrdServer
# shared library, so the tests are only built when XrdServer is being built
# (i.e. not in client-only configurations).
if(NOT TARGET XrdServer)
  return()
endif()

add_unit_tests(
  SOURCES
    XrdXrootdMonitor.cc
    XrdXrootdRedirHelper.cc
  LIBRARIES
    XrdServer
    XrdUtils
)

# The monitor statics are global to the process and Init() runs only once, so
# these tests have their own executable. The test file provides main().
add_unit_tests(monfile
  SOURCES XrdXrootdMonFile.cc
  LIBRARIES XrdServer XrdUtils
  NO_MAIN
)

# The transfer map tests need the "xfr" option and the largest buffers, which
# are global to the process, so they have their own executable too.
add_unit_tests(monxfr
  SOURCES XrdXrootdMonFileXfr.cc
  LIBRARIES XrdServer XrdUtils
  NO_MAIN
)
