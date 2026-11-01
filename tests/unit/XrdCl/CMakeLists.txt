add_unit_tests(
  SOURCES
    XrdClEnv.cc
    XrdClLocalFileHandler.cc
    XrdClPlugInManager.cc
    XrdClProtocolResp.cc
    XrdClTPFallBackCopyJob.cc
    XrdClURL.cc
    XrdClUtils.cc
  LIBRARIES
    XrdCl
    XrdTestUtils
  # The tests load the recorder plug-in from their config files
  DEPENDENCIES
    XrdClRecorder-${PLUGIN_VERSION}
)
