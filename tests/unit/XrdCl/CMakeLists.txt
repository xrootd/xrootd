add_unit_tests(
  SOURCES
    XrdClEnv.cc
    XrdClPlugInManager.cc
    XrdClPoller.cc
    XrdClProtocolResp.cc
    XrdClSocket.cc
    XrdClTPFallBackCopyJob.cc
    XrdClURL.cc
    XrdClUtils.cc
  LIBRARIES
    XrdCl
    XrdTestUtils
)

# The tests load the recorder plug-in from their config files
add_dependencies(tests-unit-xrdcl XrdClRecorder-${PLUGIN_VERSION})
