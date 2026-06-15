if(TARGET xrd)
  add_test(NAME XrdApps::xrd::stat
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/xrd-stat.sh")
  set_tests_properties(XrdApps::xrd::stat PROPERTIES
    ENVIRONMENT "XRD=$<TARGET_FILE:xrd>")
endif()
