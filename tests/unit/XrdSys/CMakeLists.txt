add_executable(xrdsys-ioevents-callback-tests IOEventsCallbackTests.cc)
target_link_libraries(xrdsys-ioevents-callback-tests
  XrdUtils GTest::gtest GTest::gtest_main)
gtest_discover_tests(xrdsys-ioevents-callback-tests
  DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 30)

# Exercise the Solaris one-shot rearm contract on every test platform.
add_executable(xrdsys-ioevents-port-tests
  IOEventsCallbackTests.cc
  IOEventsTestSupport.cc
  ../../../src/XrdSys/XrdSysIOEvents.cc
  ../../../src/XrdSys/XrdSysE2T.cc
  ../../../src/XrdSys/XrdSysPthread.cc)
target_compile_definitions(xrdsys-ioevents-port-tests PRIVATE
  XRD_SYS_IOEVENTS_FORCE_PORT_REARM)
target_link_libraries(xrdsys-ioevents-port-tests
  GTest::gtest GTest::gtest_main)
gtest_discover_tests(xrdsys-ioevents-port-tests
  TEST_PREFIX "PortContract." TEST_FILTER "IOEventsCallback.EventPort*"
  DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 30)
